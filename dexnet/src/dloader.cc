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
};


struct Context {
    zsock_t *pipe, *stream;
    int port;
    dd::config cfg{};
    BlockData data;

    Context(zsock_t* pipe, void* vargs) : pipe(pipe) {
        if (vargs) {
            cfg = *(dd::config*)vargs;
        }
        stream = zsock_new(ZMQ_PAIR);
        port = zsock_bind(stream, cfg.endpoint, NULL);
    }
    ~Context() {
        zsock_destroy(&stream);
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
    zframe_destroy(&fid);
    zsys_debug("queue_cmd %d", id);
    zframe_t* frame = zmsg_pop(msg);
    zmsg_destroy(&msg);
    if (id == dh::msg_id<dd::AskPort>()) {
        dispatch_frame<dd::AskPort>(frame, sm, dep, sub);
        return;
    }
    if (id == dh::msg_id<dd::Load>()) {
        dispatch_frame<dd::Load>(frame, sm, dep, sub);
        return;
    }
    if (id == dh::msg_id<dd::Start>()) {
        dispatch_frame<dd::Start>(frame, sm, dep, sub);
        return;
    }
    zsys_error("unexpected command: %d", id);
};

const auto back_pressure = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    
};

const auto start_send = [](Context& ctx, const auto& evin) {
    zsys_info("starting to send data");
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



struct Dloader {
    auto operator()() const noexcept {
        using namespace sml;
        return make_transition_table (
            * state<Init> = state<Idle>
            , state<Idle> + event<evPipe> / queue_cmd = state<Input>
            , state<Idle> + event<evStream> / back_pressure = state<Input>
            , state<Input> + event<dd::Term> = state<Terminate>
            , state<Input> + event<dd::AskPort> / send_port = state<Idle>
            , state<Input> + event<dd::Load> / load_data = state<Idle>
            , state<Input> + event<dd::Start> [ have_data ] / start_send  = state<Idle>
            , state<Input> + event<dd::Start> [ !have_data ] / no_send = state<Idle>
            );
    }
};



void dd::actor(zsock_t* pipe, void* vargs)
{
    Context ctx(pipe, vargs);

    zsock_signal(pipe, 0);      // ready

    dh::Logger log;
    sml::sm<Dloader, sml::logger<dh::Logger> > dm{log, ctx};
    //sml::sm<Dloader> dm{ctx};

    zpoller_t* poller = zpoller_new(ctx.pipe, ctx.stream, NULL);
    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("dloader interupted");
            break;
        }
        if (which == ctx.pipe) {
            dm.process_event(evPipe(ctx.pipe));
            continue;
        }
        if (which == ctx.stream) {
            dm.process_event(evStream(ctx.stream));
            continue;
        }
        zsys_info("dloader unknown socket");
    }
    zpoller_destroy(&poller);
}
