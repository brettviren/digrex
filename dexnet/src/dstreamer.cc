// This is like dloader but:
// 1) it's an attempt to evolve the code organization a bit.
// 2) it supports slicing the data block into N parallel, synchronous output streams.

#include "dexnet/dstreamer.h"
#include "dexnet/protohelpers.h"
#include "pb/dstreamer.pb.h"

#include <boost/sml.hpp>
#include <czmq.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include <functional>

namespace sml = boost::sml;
namespace ds = dexnet::dstreamer;
namespace dh = dexnet::helpers;

typedef std::vector<zsock_t*> socket_vector_t;
typedef std::pair<size_t, void*> vdata_t;

template<typename Sample>
struct SampleStreams {
    typedef Sample sample_t;
    sample_t* array{};
    size_t ncols{};
    size_t nrows{};
    typedef std::vector<sample_t*> excerpt_t;

    excerpt_t excerpt(size_t col, size_t row, size_t width, size_t height) {
        if (col >= ncols or col + width > ncols) { return excerpt_t(); }
        if (row >= nrows or row + height > nrows) { return excerpt_t(); }
        excerpt_t ret(height, nullptr);
        for (size_t ind=0; ind != height; ++ind) {
            const size_t irow = row + ind;
            ret[ind] = array + col + irow*ncols;
        }
        return ret;
    }
};    
typedef SampleStreams<ds::sample_t> BSS;

struct Sender {
    socket_vector_t socks{};
    BSS bss;    

    size_t nchunks{0};
    size_t nsends{0};

    int64_t started{zclock_usecs()};

    size_t ncycles{0};
    size_t cursor{0};
    size_t nsent{0};


    size_t operator()() {
        const size_t nstreams = socks.size() - 1;
        const size_t chunk = bss.ncols / nstreams;

        std::vector<BSS::excerpt_t> blocks;

        for (size_t ind=0; ind<nstreams; ++ind) {
            auto got = bss.excerpt(ind*chunk, cursor, chunk, nchunks);
            if (got.empty()) {
                if (nsends) {
                    return 0;
                }
                zsys_debug("dstreamer: sender: ending cycle %d nsent=%d",
                           ncycles, nsent);
                ++ncycles;
                cursor=0;
                return (*this)();
            }
            blocks.push_back(got);
        }
        cursor += nchunks;
        for (size_t ind=0; ind<nstreams; ++ind) {
            auto& got = blocks[ind];
            const size_t ngot = got.size();
            zmsg_t* msg = zmsg_new();

            // for now we put a place holder for any structured data.
            // probably should add some counts or something.
            // fixme: this needs to be added to some protocol.
            zframe_t* empty = zframe_new_empty();
            zmsg_append(msg, &empty);

            zframe_t* frame = zframe_new(NULL, ngot*chunk);
            byte* zdat = (byte*)zframe_data(frame);
            for (int igot=0; igot != ngot; ++igot) {
                memcpy(zdat+igot*chunk, got[igot], chunk);
            }
            zmsg_append(msg, &frame);

            zsock_t* stream = socks[ind+1];
            int rc = zmsg_send(&msg, stream);
            assert(rc == 0);
        }
        ++nsent;
        return 1;
    }

};

// actor/fsm context.
struct Context {
    socket_vector_t socks;
    zloop_t* loop;
    bool running{true};         // set to false to quit
    vdata_t vdata{};

    BSS bss;    
    std::map<int, Sender> senders;

    Context(zsock_t* pipe, void* vcfg);
    ~Context();
};



// states
struct Init {};
struct Idle {};
struct Input {};
struct Terminate {};

struct evPipe { zsock_t* sock{}; };
struct evStream { zsock_t* sock{}; };


template<typename Message, typename SM, typename Dep, typename Sub>
bool dispatch_frame(int id, zframe_t* frame, SM& sm, Dep& dep, Sub& sub)
{
    if (id != dh::msg_id<Message>()) {
        return false;
    }
    zsys_debug("dstreamer: dispatch frame id %d (%s)",
               dh::msg_id<Message>(), dh::msg_name<Message>().c_str());
    Message ev;
    dh::read_frame(frame, ev);
    zframe_destroy(&frame);
    sm.process_event(ev, dep, sub);
    return true;
}

const auto queue_cmd = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    // this converts a zmsg to an SML event which is represented by a
    // protobuf object class.  The msg ID coresponds to an event/pb
    // type.  How can I abstract this?

    zmsg_t* msg = zmsg_recv(evin.sock);
    assert(msg);                // fixme: do FSM based error handling or something
    zframe_t* fid = zmsg_pop(msg);
    int id = dh::msg_id(fid);
    zframe_destroy(&fid);

    if (dh::msg_term(id)) {
        zsys_debug("dstreamer: queue terminate");
        zmsg_destroy(&msg);
        ds::Term ev;
        sm.process_event(ev, dep, sub);
        return;
    }
    
    zsys_debug("dstreamer: queue_cmd %d", id);
    zframe_t* frame = zmsg_pop(msg);
    assert (frame);
    if (dispatch_frame<ds::Load>(id, frame, sm, dep, sub)) {/*nada*/}
    else if (dispatch_frame<ds::Start>(id, frame, sm, dep, sub)) {/*nada*/}
    else {
        zsys_error("dstreamer: unexpected command: %d 0x%x", id, id);
        // hex: 52 45 54 24
        // str:  R  E  T  $ -> $TERM
        zsys_debug("dstreamer: fid frame size [%d]", zframe_size(fid));
        assert(false);
    }

    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return;
};

const auto back_pressure = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
};

static int handle_timer(zloop_t *loop, int timer_id, void *varg)
{
    Context& ctx = *(Context*)varg;
    auto& sender = ctx.senders[timer_id];
    size_t data_sent = sender();
    if (data_sent) {
        return 0;
    }

    // sender is done.  this probably needs to raise an event.
    const size_t nsent = sender.nsent;
    zsys_debug("dstreamer: sender done %d", nsent);
    ds::Exhausted done;
    done.set_sent(nsent);
    int rc = dh::send_msg(done, ctx.socks[0]);
    assert (rc == 0);
    zloop_timer_end(ctx.loop, timer_id);
    ctx.senders.erase(timer_id);
    return 0;
}

const auto load_data = [](Context& ctx, const auto& evin) {
    auto fname = evin.filename().c_str();
    int fd = open(fname, O_RDONLY, 0);
    if (fd < 0) {
        zsys_error("dstreamer: Failed to load %s", fname);
        return;
    }
    struct stat st;
    stat(fname, &st);
    void* vdata = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
    if (ctx.vdata.second) {
        munmap(ctx.vdata.second, ctx.vdata.first);
    }
    ctx.vdata.second = vdata;
    ctx.vdata.first = st.st_size;

    const size_t offset = evin.offset();
    const size_t usable_size = (st.st_size - offset)/sizeof(ds::sample_t);
    const size_t ncols = evin.stride();
    const size_t nrows = usable_size/ncols;
    ctx.bss.array = ((ds::sample_t*)vdata)+offset;
    ctx.bss.ncols = ncols;
    ctx.bss.nrows = nrows;
    zsys_debug("dstreamer: load %d X %d", ncols, nrows);
};

const auto start_send = [](Context& ctx, const auto& evin) {
    // guard makes sure we aren't already sending
    int id = zloop_timer(ctx.loop, evin.delay(), 0, handle_timer, &ctx);
    ctx.senders[id] = Sender{ ctx.socks, ctx.bss,
                              evin.nchunks(), evin.nsends()};
    zsys_debug("dstreamer: start send on timer id #%d nchunks=%d, nsends=%d",
               id, evin.nchunks(), evin.nsends());
};
const auto no_start = [](Context& ctx, const auto& evin) {
    zsys_info("dstreamer: start ignored");
};

const auto shut_down = [](Context& ctx, const auto& evin) {
    zsys_debug("dstreamer: shutting down");
    ctx.running = false;
    // do anything here?
};

// guards
auto ready_data = [](Context& ctx) {
    return ctx.vdata.second != nullptr;
};

struct FSM {

    auto operator()() const noexcept {
        using namespace boost::sml;
        return make_transition_table (
            * state<Init> = state<Idle>
            , state<Idle> + event<evPipe> / queue_cmd = state<Input>
            , state<Idle> + event<evStream> / back_pressure = state<Input>
            , state<Input> + event<ds::Term> / shut_down = state<Terminate>
            , state<Input> + event<ds::Load> / load_data = state<Idle>
            , state<Input> + event<ds::Start> [ ready_data ] / start_send  = state<Idle>
            , state<Input> + event<ds::Start> [ !ready_data ] / no_start = state<Idle>
            , state<Terminate> = X
            );            
    }
};


Context::Context(zsock_t* pipe, void* vargs)
{
    socks.push_back(pipe);      // zero sock is special = control pipe.
    ds::config& cfg = *(ds::config*)vargs;
    for (const auto& ep : cfg.endpoints) {
        zsock_t* stream = zsock_new(ZMQ_PAIR);
        assert(stream);
        int port = zsock_bind(stream, ep.c_str(), NULL);
        assert (port >= 0);
        socks.push_back(stream);
    }
    loop = zloop_new();
    assert(loop);

}
Context::~Context()
{
    // skip pipe in ind=0
    for (size_t ind=1; ind<socks.size(); ++ind) {
        zsock_destroy(&socks[ind]);
    }
    zloop_destroy(&loop);
}


typedef sml::sm<FSM> FSM_t;
struct HandleSock_t { FSM_t& fsm; Context& ctx; };
static int handle_sock (zloop_t */*loop*/, zsock_t *stream, void *vobj)
{
    HandleSock_t* hs = (HandleSock_t*)vobj;
    
    if (stream == hs->ctx.socks[0]) {
        hs->fsm.process_event(evPipe{stream});
    }
    else {
        hs->fsm.process_event(evStream{stream});
    }
    return hs->ctx.running ? 0 : -1;
}


void ds::actor(zsock_t* pipe, void* vargs)
{
    Context ctx(pipe, vargs);
    FSM_t fsm{ctx};

    // need both in handler, maybe there's a way to get ctx from fsm?
    HandleSock_t both{fsm, ctx};

    zsock_signal(pipe, 0);      // ready

    for (int ind=0; ind < ctx.socks.size(); ++ind) {
        zsock_t* sock = ctx.socks[ind];
        int rc = zloop_reader(ctx.loop, sock, handle_sock, &both);
        assert (rc == 0);
        zloop_reader_set_tolerant (ctx.loop, sock);
    }
    
    zloop_start(ctx.loop);
}
