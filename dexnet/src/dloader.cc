#include "dexnet/dloader.h"
#include "dexnet/smlhelpers.h"
#include "dexnet/protohelpers.h"
#include "pb/dloader.pb.h"

#include "boost/sml.hpp"

namespace sml = boost::sml;
namespace dd = dexnet::dloader;
namespace dh = dexnet::helpers;


struct BlockData {
    short int* array{nullptr};
    size_t bytes{0};            // size in bytes
    size_t offset{0};           // location in array
    size_t stride{0};           // how far to advance after taking a chunk
    size_t chunk{0};            // size of chunk
    size_t end{0};              // one past last index of array.

    // for controlling streaming of the data
    unsigned int nchunks{0};
    unsigned int nsent{0};

    // stats
    int64_t tbeg{0};
};
struct Context {
    zsock_t *pipe, *stream;
    zloop_t* loop;
    int port;
    int timer_id{-1};
    dd::config cfg{};
    BlockData data{};

    // set when it's time to stop the loop.
    bool die{false};
    
    Context(zsock_t* pipe, void* vargs) : pipe(pipe) {
        if (vargs) {
            cfg = *(dd::config*)vargs;
        }
        stream = zsock_new(ZMQ_PAIR);
        assert(stream);
        port = zsock_bind(stream, cfg.endpoint, NULL);
        assert (port >= 0);
        loop = zloop_new();
        assert(loop);
        // It's up to the user of Context to connect loop handlers
    }


    ~Context() {
        zsock_destroy(&stream);
        zloop_destroy(&loop);
    }
};

// Events
struct evPipe {
    zsock_t* sock;
    evPipe(zsock_t* s) : sock(s) {}
};
struct evStream {
    zsock_t* sock;
    evStream(zsock_t* s) : sock(s) {}
};

// States
struct Init {};
struct Idle {};
struct Input {};
struct Terminate {};

const auto shut_down = [](Context& ctx, const auto& evin) {
    zsys_debug("dloader: shutting down");
    ctx.die = true;
    // do anything here?
//    zsys_shutdown();
};
const auto send_port = [](Context& ctx, const auto& evin) {
    zsys_debug("dloader: send port");
    dd::Port portmsg;
    portmsg.set_port(ctx.port);
    zmsg_t* msg = dh::make_msg(portmsg);
    int rc = zmsg_send(&msg, ctx.pipe);
    assert(rc == 0);
};

template<typename Message, typename SM, typename Dep, typename Sub>
void dispatch_frame(zframe_t* frame, SM& sm, Dep& dep, Sub& sub)
{
    zsys_debug("dloader: dispatch frame id %d (%s)", dh::msg_id<Message>(), dh::msg_name<Message>().c_str());
    Message ev;
    dh::read_frame(frame, ev);
    zframe_destroy(&frame);
    sm.process_event(ev, dep, sub);
}

const auto queue_cmd = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    zmsg_t* msg = zmsg_recv(evin.sock);
    assert(msg);                // fixme: do FSM based error handling or something
    zframe_t* fid = zmsg_pop(msg);
    auto id = dh::msg_id(fid);
    if (dh::msg_term(id)) {
        zsys_debug("dloader: queue terminate");
        dispatch_frame<dd::Term>(fid, sm, dep, sub);
        return;
    }

    zsys_debug("dloader: queue_cmd %d", id);
    zframe_t* frame = zmsg_pop(msg);
    assert (frame);
    if (id == dh::msg_id<dd::AskPort>()) {
        dispatch_frame<dd::AskPort>(frame, sm, dep, sub);
    }
    else if (id == dh::msg_id<dd::Load>()) {
        dispatch_frame<dd::Load>(frame, sm, dep, sub);
    }
    else if (id == dh::msg_id<dd::Start>()) {
        dispatch_frame<dd::Start>(frame, sm, dep, sub);
    }
    else {
        zsys_error("dloader: unexpected command: %d 0x%x", id, id);
        // hex: 52 45 54 24
        // str:  R  E  T  $ -> $TERM
        zsys_debug("dloader: fid frame size [%d]", zframe_size(fid));
        assert(false);
    }

    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return;
};

const auto back_pressure = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    
};

int handle_timer(zloop_t *loop, int timer_id, void *varg)
{
    // fixme: this handler operates outside the jurisdiction of the
    // FSM.

    Context& ctx = *(Context*)varg;

    // fixme: this is a bug if stride < chunk.
    if (ctx.data.offset + ctx.data.nchunks*ctx.data.stride > ctx.data.end) { // would be short read
        zsys_info("dloader: stream end after sending %d in %f s", ctx.data.nsent, 1e-6*(zclock_usecs() - ctx.data.tbeg));
        zloop_timer_end(ctx.loop, ctx.timer_id);
        ctx.timer_id = -1;

        dd::Exhausted done;
        done.set_sent(ctx.data.nsent);
        int rc = dh::send_msg(done, ctx.pipe);
        assert (rc == 0);
        return 0;
    }
   // else {
   //     size_t nleft = (ctx.data.end-ctx.data.offset)/(ctx.data.stride*ctx.data.nchunks);
   //     zsys_debug("send: %jd, left: %jd", ctx.data.nsent, nleft);
   // }
    
    zmsg_t* msg = zmsg_new();

    // the idea is that we send a stream of nchunks each of length
    // chunk and separated by stride.

    const size_t chunk_bytes = sizeof(short int)*ctx.data.chunk;
    zframe_t* frame = zframe_new(NULL, chunk_bytes * ctx.data.nchunks);
    // now iterate the frame by chunks and the data array by strides
    for (size_t ind=0; ind<ctx.data.nchunks; ++ind) {
        memcpy(zframe_data(frame) + ind*chunk_bytes,
               ctx.data.array + ctx.data.offset,
               chunk_bytes);
        ctx.data.offset += ctx.data.stride;
    }
    zmsg_append(msg, &frame);
    int rc = zmsg_send(&msg, ctx.stream);
    assert (rc == 0);
    ++ctx.data.nsent;
    return 0;
}


const auto start_send = [](Context& ctx, const auto& evin) {
    // guard makes sure we aren't already sending

    int delay = evin.delay();
    ctx.data.nchunks = evin.nchunks();

    size_t nsends = (ctx.data.end - ctx.data.offset)/(ctx.data.stride*ctx.data.nchunks);

    zsys_info("dloader: starting to send data, expect %d", nsends);

    ctx.data.tbeg = zclock_usecs();
    ctx.timer_id = zloop_timer(ctx.loop, delay, 0, handle_timer, &ctx);

};
const auto no_send = [](Context& ctx, const auto& evin) {
    zsys_info("dloader: start ignored");
};


const auto load_data = [](Context& ctx, const auto& evin) {
    auto tbeg = zclock_usecs();
    auto fname = evin.filename().c_str();
    int fd = open(fname, O_RDONLY, 0);
    if (fd < 0) {
        zsys_error("dloader: Failed to load %s", fname);
        return;
    }
    struct stat st;
    stat(fname, &st);
    void* vdata = malloc(st.st_size);
    if (!vdata) {
        zsys_error("dloader: Failed to allocate %d bytes", st.st_size);
        return;
    }
    int rc = read(fd, vdata, st.st_size);
    if (rc < 0) {
        zsys_error("dloader: Failed to read %d bytes from %s", st.st_size, fname);
        free(vdata);
        return;
    }
    if (ctx.data.array) {
        free(ctx.data.array);
    }
    ctx.data.array = (short int*) vdata;
    ctx.data.bytes = st.st_size;
    ctx.data.offset = evin.offset();
    ctx.data.stride = evin.stride();
    ctx.data.chunk = evin.chunk();
    ctx.data.end = ctx.data.bytes/sizeof(short int) - ctx.data.offset;

    size_t nstrides = (ctx.data.end - ctx.data.offset)/ctx.data.stride;

    auto tend = zclock_usecs();
    zsys_info("dloader: load \"%s\" in %.fs bytes=%jd offset=%jd stride=%jd chunk=%jd end=%jd, nstrides=%jd",
              fname, 1e-6*(tend-tbeg), ctx.data.bytes, ctx.data.offset,
              ctx.data.stride, ctx.data.chunk,
              ctx.data.end, nstrides);

    return;
};



// guards
auto have_data = [](Context& ctx) {
    if (ctx.data.array == nullptr) { return false; }
    return true;                                     
};

auto paused = [](Context& ctx) {
    return ctx.timer_id < 0;
};

struct Dloader {
    auto operator()() const noexcept {
        using namespace sml;
        return make_transition_table (
            * state<Init> = state<Idle>
            , state<Idle> + event<evPipe> / queue_cmd = state<Input>
            , state<Idle> + event<evStream> / back_pressure = state<Input>
            , state<Input> + event<dd::Term> / shut_down = state<Terminate>
            , state<Input> + event<dd::AskPort> / send_port = state<Idle>
            , state<Input> + event<dd::Load> / load_data = state<Idle>
            , state<Input> + event<dd::Start> [ have_data and paused ] / start_send  = state<Idle>
            , state<Input> + event<dd::Start> [ !have_data ] / no_send = state<Idle>
            , state<Terminate> = X
            );
    }
};

#if LOG_FSM
typedef sml::sm<Dloader, sml::logger<dh::Logger> > TopSM;
#else
typedef sml::sm<Dloader> TopSM;
#endif
struct CtxFsm {
    Context& ctx;
    TopSM& fsm;
    CtxFsm(Context& ctx, TopSM& fsm) : ctx(ctx), fsm(fsm) {}
};

int handle_pipe (zloop_t *loop, zsock_t *pipe, void *vobj)
{
    CtxFsm& both = *(CtxFsm*)vobj;
    //zsys_debug("got pipe");
    both.fsm.process_event(evPipe(pipe));

    if (both.ctx.die) {
        zsys_debug("dloader: shutting down from pipe handler");
        return -1;
    }
    return 0;
}

int handle_stream (zloop_t *loop, zsock_t *stream, void *vobj)
{
    CtxFsm& both = *(CtxFsm*)vobj;
    zsys_debug("dloader: got stream");
    both.fsm.process_event(evStream(stream));

    if (both.ctx.die) {
        zsys_debug("dloader: shutting down from stream handler");
        return -1;
    }
    return 0;
}


void dd::actor(zsock_t* pipe, void* vargs)
{
    Context ctx(pipe, vargs);
#if LOG_FSM
    dh::Logger log;
    TopSM dm{log, ctx};
#else
    TopSM dm{ctx};
#endif
    //sml::sm<Dloader> dm{ctx};
    CtxFsm both(ctx, dm);

    zsys_debug("dloader: dloader going ready");
    zsock_signal(pipe, 0);      // ready

    {
        int rc = zloop_reader(ctx.loop, ctx.pipe, handle_pipe, &both);
        assert (rc == 0);
        zloop_reader_set_tolerant (ctx.loop, ctx.pipe);
        zsys_debug("dloader: looping pipe");
    }
    {
        int rc = zloop_reader(ctx.loop, ctx.stream, handle_stream, &both);
        assert (rc == 0);
        zloop_reader_set_tolerant (ctx.loop, ctx.stream);
        zsys_debug("dloader: looping steam");
    }

    zloop_start(ctx.loop);

}
