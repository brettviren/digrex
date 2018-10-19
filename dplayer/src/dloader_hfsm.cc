#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/common.hpp>
#include <boost/msm/front/euml/state_grammar.hpp>

#include <czmq.h>

#include <cxxabi.h>

template<class T>
const char* whatis(const T& t)
{
    const char* tname = typeid(t).name();
    int status = -4;
    char* ret = abi::__cxa_demangle(tname, NULL, NULL, &status);
    if (ret) {
        return ret;
    }
    return tname;
}

namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;            

// events

struct ev_input {
    void* which = nullptr;      // something like a pointer to socket
    zsock_t* pipe;
    ev_input(void* which, zsock_t* pipe) : which(which), pipe(pipe) {}
};
struct ev_pipe {                // something happened on the pipe
    zsock_t* sock=nullptr;
    ev_pipe(zsock_t* sock): sock(sock) {}
};
struct ev_socketok {};          // a socket was handled okay

struct ev_term {};              // a $TERM "message" was sent
struct ev_terminate {};         // a terminate event


struct Chirp_Entry
{
    template <class Event,class FSM,class STATE>
    void operator()(Event const& evt,FSM&,STATE& state) {
        zsys_debug("entry: \n\tevent:%s\n\tstate:%s", whatis(evt), whatis(state));
    }
};
struct Chirp_Exit
{
    template <class Event,class FSM,class STATE>
    void operator()(Event const& evt,FSM&,STATE& state) {
        zsys_debug("exit: \n\tevent:%s \n\tstate:%s", whatis(evt), whatis(state));
    }
};

// actions
struct send_socketok {
    template <class EVT,class FSM,class SourceState,class TargetState>
    void operator()(EVT const& ev ,FSM& fsm, SourceState& , TargetState& ) {
        fsm.process_event(ev_socketok());
    }
};
struct send_terminate {
    template <class EVT,class FSM,class SourceState,class TargetState>
    void operator()(EVT const& ev ,FSM& fsm, SourceState& , TargetState& ) {
        fsm.process_event(ev_terminate());
    }
};
struct forward_event {
    template <class EVT,class FSM,class SourceState,class TargetState>
    void operator()(EVT const& ev ,FSM& fsm, SourceState& , TargetState& ) {
        fsm.process_event(ev);
    }
};

struct parse_pipe {
    template <class EVT,class FSM,class SourceState,class TargetState>
    void operator()(EVT const& ev ,FSM& fsm, SourceState& , TargetState& ) {
        zmsg_t *msg = zmsg_recv (ev.sock);
        char *command = zmsg_popstr (msg);
        if (streq (command, "$TERM")) {
            zsys_debug("parse_pipe: push ev_term");
            fsm.process_event(ev_term());
        }
        else {
            zsys_error("command not implemented: %s", command);
        }
        free (command);
        zmsg_destroy (&msg);
    }
};
struct parse_input {
    template <class EVT,class FSM,class SourceState,class TargetState>
    void operator()(EVT const& ev ,FSM& fsm, SourceState& , TargetState& ) {
        if (!ev.which) {
            zsys_debug("parse_input: push ev_terminate");
            fsm.process_event(ev_terminate());
            return;
        }
        if (ev.which == ev.pipe) {
            zsys_debug("parse_input: push ev_pipe");
            fsm.process_event(ev_pipe(ev.pipe));
            return;
        }
    }
};



struct HandlingPipe_ : public msm::front::state_machine_def<HandlingPipe_>
{
    
    struct ReceivingPipe_tag {};
    typedef msm::front::euml::func_state<ReceivingPipe_tag, Chirp_Entry, Chirp_Exit> ReceivingPipe;
    struct HandlingTerm_tag {};
    typedef msm::front::euml::func_state<HandlingTerm_tag, Chirp_Entry, Chirp_Exit> HandlingTerm;

    typedef ReceivingPipe initial_state;    

    struct transition_table : mpl::vector<
        Row <ReceivingPipe, ev_pipe, ReceivingPipe, parse_pipe>,
        Row <ReceivingPipe, ev_term, HandlingTerm>
        > {};

};
typedef boost::msm::back::state_machine<HandlingPipe_> HandlingPipe;


struct Dloader_ : public msm::front::state_machine_def<Dloader_>
{
    struct Idle_tag {};
    typedef msm::front::euml::func_state<Idle_tag, Chirp_Entry, Chirp_Exit> Idle;
    struct Stop_tag {};
    typedef msm::front::euml::func_state<Stop_tag, Chirp_Entry, Chirp_Exit> Stop;

    typedef Idle initial_state;    

    //    Start | Event | Next | Action | Guard
    struct transition_table : mpl::vector<
        Row < Idle, ev_input, Idle, parse_input>,
        Row < Idle, ev_pipe, HandlingPipe, forward_event>,
        Row < HandlingPipe, none, Idle>
        > {};

};
typedef msm::back::state_machine<Dloader_> Dloader;


void dloader_hfsm(zsock_t* pipe, void* vargs)
{
    Dloader fsm;
    
    zsock_signal(pipe, 0);      // ready    

    zpoller_t* poller = zpoller_new(pipe, NULL);

    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("interrupted");
            break;
        }
        fsm.process_event(ev_input(which, pipe));
    }

    zpoller_destroy(&poller);

}
