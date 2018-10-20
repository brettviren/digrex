#include "boost/sml.hpp"

#include "pb/dloader.pb.h"

#include <czmq.h>

#include <variant>
#include <typeinfo>
#include <cxxabi.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>


template<typename T>
const char* tname(const T& o)
{
    int status=0;
    return abi::__cxa_demangle(typeid(o).name(), NULL, NULL, &status);
}


namespace sml = boost::sml;

//
// messages
//

namespace MsgId {
    enum DloaderMsgId {
        unknown, askport, port, load, start
    };
}

namespace dexnet {
    namespace dloader {
        typedef std::variant<void*, AskPort, Port, Load, Start> msgtypes;
    }
}

template<typename ProtoBuftype>
zframe_t* make_frame(const ProtoBuftype& obj)
{
    zframe_t* frame = zframe_new(NULL, obj.ByteSize());
    obj.SerializeToArray(zframe_data(frame), zframe_size(frame));
    return frame;
}
template<typename ProtoBuftype>
void read_frame(zframe_t* frame, ProtoBuftype& obj)
{
    obj.ParseFromArray(zframe_data(frame), zframe_size(frame));
}

template<typename ProtoBuftype>
int send_message(zsock_t* sock, const ProtoBuftype& obj)
{
    dexnet::dloader::msgtypes which = obj;
    int id = which.index();
    zmsg_t* msg = zmsg_new();
    zmsg_addmem(msg, &id, sizeof(int));
    zframe_t* frame = make_frame(obj);
    zmsg_append(msg, &frame);
    return zmsg_send(&msg, sock);
}

// this pops the ID frame
MsgId::DloaderMsgId msg_type(zmsg_t* msg)
{
    zframe_t* frame = zmsg_pop(msg);
    if (!frame) { return MsgId::unknown; }
    if (zframe_size(frame) != sizeof(int)) {return MsgId::unknown; }
    int id = *(int*)zframe_data(frame);
    zframe_destroy(&frame);
    return (MsgId::DloaderMsgId)id;
}



//
// Context
//

template<typename NumberType>
struct DataBlock {
    NumberType* array{nullptr};
    size_t bytes{0};
    size_t offset{0};
    size_t stride{0};
    size_t chunk{0};
};

struct DloaderContext {
    DataBlock<short int> data;
    zsock_t *in{nullptr}, *out{nullptr};
    int port{-1};
    int timer_id{0};
    DloaderContext(zsock_t* inpipe, zsock_t* outpipe)
        : in(inpipe), out(outpipe) {}
};

struct Logger {
    template <class SM, class TEvent>
    void log_process_event(const TEvent& ev) {
        zsys_debug("[%s][process_event] %s",
                   tname(SM{}), tname(ev));
    }

    template <class SM, class TGuard, class TEvent>
    void log_guard(const TGuard& g, const TEvent& ev, bool result) {
        zsys_debug("[%s][guard] %s %s %s",
                   tname(SM{}), tname(g), tname(ev),
                   (result ? "[OK]" : "[Reject]"));
    }

    template <class SM, class TAction, class TEvent>
    void log_action(const TAction& a, const TEvent& ev) {
        zsys_debug("[%s][action] %s %s",
                   tname(SM{}), tname(a), tname(ev));
    }

    template <class SM, class TSrcState, class TDstState>
    void log_state_change(const TSrcState& src, const TDstState& dst) {
        zsys_debug("[%s][transition] %s -> %s",
                   tname(SM{}), src.c_str(), dst.c_str());
    }
};

    
// states
struct Init {};
struct Idle {};
struct HandleInput {};
struct Terminate {};
struct StartSend {};

// events
struct evInput {
    zsock_t* sock;
    evInput(zsock_t* pipe) : sock(pipe) {
    }
};
struct evTerm {
    evTerm(zframe_t* frame) {
    }
};
struct evAskPort {
    evAskPort(zframe_t* frame) {
    }
};
struct evLoad {
    dexnet::dloader::Load cfg;
    evLoad(zframe_t* frame) { read_frame(frame, cfg); }
};
struct evStart {
    dexnet::dloader::Start cfg;
    evStart(zframe_t* frame) { read_frame(frame, cfg); }

};

//
// Actions
//

//
// Convert input into event
//
const auto queue_cmd = [](const auto& evin, auto& sm, auto& dep, auto& sub) {
    zmsg_t* msg = zmsg_recv(evin.sock);
    assert(msg);                // fixme: do FSM based error handling or something
    auto id = msg_type(msg);
    zsys_debug("queue_cmd %d", id);
    zframe_t* frame = zmsg_pop(msg);
    zmsg_destroy(&msg);
    switch(id) {
    default: break;
    case MsgId::askport: {
        evAskPort ev(frame);
        zframe_destroy(&frame);
        sm.process_event(ev, dep, sub);
        break;
    }
    case MsgId::load: {
        evLoad ev(frame);
        zframe_destroy(&frame);
        sm.process_event(ev, dep, sub);
        break;
    }
    case MsgId::start: {
        evStart ev(frame);
        zframe_destroy(&frame);
        sm.process_event(ev, dep, sub);
        break;
    }
    }
};


const auto send_port = [](DloaderContext& ctx, const auto& evin) {
    zsys_debug("send port");
    dexnet::dloader::Port portmsg;
    portmsg.set_port(ctx.port);
    send_message(ctx.in, portmsg);
};

// fixme: do some real error handling here, not just chirping to log
const auto load_data = [](DloaderContext& ctx, const auto& evin) {
    auto fname = evin.cfg.filename().c_str();
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
    ctx.data.offset = evin.cfg.offset();
    ctx.data.stride = evin.cfg.stride();
    ctx.data.chunk = evin.cfg.chunk();
    return;
};


const auto start_send = [](DloaderContext& ctx, const auto& evin) {
    zsys_info("starting to send data");
};
const auto no_send = [](DloaderContext& ctx, const auto& evin) {
    zsys_info("start ignored");
};



// guards
auto have_data = [](DloaderContext& ctx) {
    if (ctx.data.array == nullptr) { return false; }
    return true;                                     
};


struct DloaderMachine {
    auto operator()() const noexcept {
        using namespace sml;
        return make_transition_table (
            * state<Init> = state<Idle>
            , state<Idle> + event<evInput> / queue_cmd = state<HandleInput>
            , state<HandleInput> + event<evTerm> = state<Terminate>
            , state<HandleInput> + event<evAskPort> / send_port = state<Idle>
            , state<HandleInput> + event<evLoad> / load_data = state<Idle>
            , state<HandleInput> + event<evStart> [ have_data ] / start_send  = state<Idle>
            , state<HandleInput> + event<evStart> [ !have_data ] / no_send = state<Idle>
            , state<StartSend> = state<Idle>
            );
    }
};


void dloader(zsock_t* pipe, void* vargs)
{
    const char* output_endpoint = (const char*) vargs;
    zsock_t* out = zsock_new(ZMQ_PAIR);
    int port = zsock_bind(out, output_endpoint, NULL);

    DloaderContext ctx(pipe, out);
    ctx.port = port;

    zsock_signal(pipe, 0);

    zpoller_t* poller = zpoller_new(pipe, NULL);
    
    //sml::sm<DloaderMachine> dm{ctx};
    Logger log;

    sml::sm<DloaderMachine, sml::logger<Logger> > dm{log, ctx};

    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("bye bye");
            break;
        }
        dm.process_event(evInput(pipe));
    }

}

int main()
{
    zsys_init();
    zsys_set_logident("dloader");

    zactor_t* loader = zactor_new(dloader, (void*)"inproc://loader");

    send_message(zactor_sock(loader), dexnet::dloader::AskPort{});

    // this should do nothing as we've not yet loaded
    dexnet::dloader::Start start;
    start.set_delay(1);
    start.set_nchunks(1000);
    send_message(zactor_sock(loader), start);

    dexnet::dloader::Load load;
    load.set_filename("test_dloader.dat");
    load.set_offset(0);
    load.set_stride(480);
    load.set_chunk(480);
    send_message(zactor_sock(loader), load);

    send_message(zactor_sock(loader), start);

    zpoller_t* poller = zpoller_new(zactor_sock(loader), loader, NULL);
    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("bye bye");
            break;
        }
        if (which != zactor_sock(loader)) {
            zsys_error("unexpected socket");
            continue;
        }
        zmsg_t* msg = zmsg_recv(which);
        auto id = msg_type(msg);
        zsys_info("message %d", id);
        switch (id) {
        default: break; 
        case MsgId::port: {
            dexnet::dloader::Port portmsg;
            zframe_t* frame = zmsg_pop(msg);
            read_frame(frame, portmsg);
            zframe_destroy(&frame);
            zsys_info("\tport = %d", portmsg.port());
            break;}
        }

    }

    zactor_destroy(&loader);
    return 0;
}
