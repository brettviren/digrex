#include "boost/sml.hpp"
#include <czmq.h>

namespace sml = boost::sml;

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
};

// states
struct Init {};
struct Idle {};
struct HandleInput {};
struct Terminate {};
struct SendPort {};
struct LoadData {};
struct StartSend {};

// events
struct evInput {
    zsock_t* sock;
    evInput(zsock_t* pipe) : sock(pipe) {}
};
struct evTerm {};
struct evPort {};
struct evLoad {};
struct evStart {};

// actions

// peek message to see command, raise command as event
const auto queue_cmd = [](DloaderContext& ctx) {
};

// guards
const auto have_data = [](DloaderContext& ctx) { return true; };


struct DloaderMachine {
    auto operator()() const noexcept {
        return sml::make_transition_table (
            * sml::state<Init> = sml::state<Idle>
            , sml::state<Idle> + sml::event<evInput> / queue_cmd = sml::state<HandleInput>
            , sml::state<HandleInput> + sml::event<evTerm> = sml::state<Terminate>
            , sml::state<HandleInput> + sml::event<evPort> = sml::state<SendPort>
            , sml::state<HandleInput> + sml::event<evLoad> = sml::state<LoadData>
            , sml::state<HandleInput> + sml::event<evStart> [ have_data] = sml::state<StartSend>
            );
    }
};

void dloader(zsock_t* pipe, void* vargs)
{
    zsock_signal(pipe, 0);

    zpoller_t* poller = zpoller_new(pipe, NULL);
    
    DloaderContext ctx;
    sml::sm<DloaderMachine> dm{ctx};

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
    zactor_t* loader = zactor_new(dloader, NULL);
    zactor_destroy(&loader);


    return 0;
}
