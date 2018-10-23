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
    size_t bytes{0};
    size_t offset{0};
    size_t stride{0};
    size_t chunk{0};

    // for controlling streaming of the data
    unsigned int delay{0};
    unsigned int nchunks{0};
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
    zsys_debug("shutting down");
    ctx.die = true;
    // do anything here?
//    zsys_shutdown();
};
const auto send_port = [](Context& ctx, const auto& evin) {
    zsys_debug("send port");
    dd::Port portmsg;
    portmsg.set_port(ctx.port);
    zmsg_t* msg = dh::make_msg(portmsg);
    int rc = zmsg_send(&msg, ctx.pipe);
    assert(rc == 0);
};

template<typename Message, typename SM, typename Dep, typename Sub>
void dispatch_frame(zframe_t* frame, SM& sm, Dep& dep, Sub& sub)
{
    zsys_debug("dispatch frame id %d (%s)", dh::msg_id<Message>(), dh::msg_name<Message>().c_str());
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
        zsys_debug("queue terminate");
        dispatch_frame<dd::Term>(fid, sm, dep, sub);
        return;
    }

    zsys_debug("queue_cmd %d", id);
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
        zsys_error("unexpected command: %d 0x%x", id, id);
        // hex: 52 45 54 24
        // str:  R  E  T  $ -> $TERM
        zsys_debug("fid frame size [%d]", zframe_size(fid));
        assert(false);
    }

    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return;
};

const auto back_pressure = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    
};

const auto start_send = [](Context& ctx, const auto& evin) {
    zsys_info("starting to send data");
    ctx.data.delay = evin.delay();
    ctx.data.nchunks = evin.nchunks();

};
const auto no_send = [](Context& ctx, const auto& evin) {
    zsys_info("start ignored");
};


const auto load_data = [](Context& ctx, const auto& evin) {
    auto fname = evin.filename().c_str();
    int fd = open(fname, O_RDONLY, 0);
    if (fd < 0) {
        zsys_error("Failed to load %s", fname);
        return;
    }
    struct stat st;
    stat(fname, &st);
    void* vdata = malloc(st.st_size);
    if (!vdata) {
        zsys_error("Failed to allocate %d bytes", st.st_size);
        return;
    }
    int rc = read(fd, vdata, st.st_size);
    if (rc < 0) {
        zsys_error("Failed to read %d bytes from %s", st.st_size, fname);
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

typedef sml::sm<Dloader, sml::logger<dh::Logger> > TopSM;
struct CtxFsm {
    Context& ctx;
    TopSM& fsm;
    CtxFsm(Context& ctx, TopSM& fsm) : ctx(ctx), fsm(fsm) {}
};

int handle_pipe (zloop_t *loop, zsock_t *pipe, void *vobj)
{
    CtxFsm& both = *(CtxFsm*)vobj;
    zsys_debug("got pipe");
    both.fsm.process_event(evPipe(pipe));

    if (both.ctx.die) {
        zsys_debug("shutting down from pipe handler");
        return -1;
    }
    return 0;
}

int handle_stream (zloop_t *loop, zsock_t *stream, void *vobj)
{
    CtxFsm& both = *(CtxFsm*)vobj;
    zsys_debug("got stream");
    both.fsm.process_event(evStream(stream));

    if (both.ctx.die) {
        zsys_debug("shutting down from stream handler");
        return -1;
    }
    return 0;
}


void dd::actor(zsock_t* pipe, void* vargs)
{
    Context ctx(pipe, vargs);

    dh::Logger log;
    TopSM dm{log, ctx};
    //sml::sm<Dloader> dm{ctx};
    CtxFsm both(ctx, dm);

    zsys_debug("dloader going ready");
    zsock_signal(pipe, 0);      // ready

    {
        int rc = zloop_reader(ctx.loop, ctx.pipe, handle_pipe, &both);
        assert (rc == 0);
        zloop_reader_set_tolerant (ctx.loop, ctx.pipe);
        zsys_debug("looping pipe");
    }
    {
        int rc = zloop_reader(ctx.loop, ctx.stream, handle_stream, &both);
        assert (rc == 0);
        zloop_reader_set_tolerant (ctx.loop, ctx.stream);
        zsys_debug("looping steam");
    }

    zloop_start(ctx.loop);

    // zpoller_t* poller = zpoller_new(ctx.pipe, ctx.stream, NULL);
    // while (true) {
    //     void* which = zpoller_wait(poller, -1);
    //     if (!which) {
    //         zsys_info("dloader interupted");
    //         break;
    //     }
    //     if (which == ctx.pipe) {
    //         dm.process_event(evPipe(ctx.pipe));
    //         continue;
    //     }
    //     if (which == ctx.stream) {
    //         dm.process_event(evStream(ctx.stream));
    //         continue;
    //     }
    //     zsys_info("dloader unknown socket");
    // }
    // zpoller_destroy(&poller);
}
