#include "boost/sml.hpp"
#include <cassert>
#include <typeinfo>
#include <czmq.h>
#include <vector>

#include <cxxabi.h>

template<typename T>
const char* tname(const T& o)
{
    int status=0;
    return abi::__cxa_demangle(typeid(o).name(), NULL, NULL, &status);
}

namespace sml = boost::sml;

typedef int cmd_id;
typedef int pipe_id;

// state
struct Context {
    int nprocessed{0};
    pipe_id pipes[3] = {0,1,2};         // would be zsock_t*'s
    cmd_id cmd{0};                      // would be a zmsg_t*
};


// events
struct ev_ok { };
struct ev_input {
    pipe_id pipe{0};
    cmd_id cmd{0};
};
struct ev_cmd1 {};
struct ev_cmd2 {};

// states
struct Dloader {};
struct ReadingPipe {};
struct DoingCmd1 {};
struct DoingCmd2 {};



// actions
const auto increment_procs = [](Context& ctx) { ++ctx.nprocessed; };

const auto send_cmd = [](const auto& ev, auto& sm, auto& dep, auto& sub) {
    zsys_debug("send cmd %d", ev.cmd);
    if (ev.cmd == 1) {
        sm.process_event(ev_cmd1{}, dep, sub);
    }
    if (ev.cmd == 2) {
        sm.process_event(ev_cmd2{}, dep, sub);
    }
};

const auto chirp = [](const auto& ev, auto& sm, auto& dep, auto& sub) {
    zsys_debug("ev:\t%s", tname(ev));
    zsys_debug("sm:\t%s", tname(sm));
    zsys_debug("dep:\t%s", tname(dep));
    zsys_debug("sub:\t%s", tname(sub));
};

// guards

const auto is_pipe = [](const auto& ev) { return ev.pipe > 0; };

struct is_cmd {
    cmd_id want{0};
    is_cmd(cmd_id want) : want(want) {}
    bool operator()(const ev_input& ev) { return ev.cmd == want; }
};




struct Machine {

    auto operator()() const noexcept {
        auto dloader = sml::state<Dloader>;
        auto readingpipe = sml::state<ReadingPipe>;
        auto docmd1 = sml::state<DoingCmd1>;
        auto docmd2 = sml::state<DoingCmd2>;

        return sml::make_transition_table (
            * dloader + sml::event<ev_input> [ is_pipe ] / send_cmd = readingpipe
            , readingpipe + sml::event<ev_cmd1> / increment_procs = docmd1
            , readingpipe + sml::event<ev_cmd2> / increment_procs = docmd2
            , docmd1 + sml::on_entry<ev_cmd1> / chirp
            , docmd1 = sml::X
            , docmd2 = sml::X
            );
    }
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


int main()
{
    zsys_init();
    zsys_set_logident("test_sml");

    Context ctx;

    Logger log;

    for (cmd_id cmd : std::vector<cmd_id>{1,2}) {
        //sml::sm<Machine, sml::logger<Logger>> sm{log, ctx};
        sml::sm<Machine> sm{ctx};
        sm.process_event(ev_input{pipe_id{1}, cmd_id{cmd}});
        assert(sm.is(sml::X));
    }


    zsys_debug("%d processed", ctx.nprocessed);



    return 0;
}
