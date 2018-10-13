#include <vector>
#include <iostream>
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
// functors
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/common.hpp>
// for And_ operator
#include <boost/msm/front/euml/operator.hpp>
// for func_state and func_state_machine
#include <boost/msm/front/euml/state_grammar.hpp>
using namespace std;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;
// for And_ operator
//using namespace msm::front::euml;

namespace {

    // events
    struct hello_event {};
    struct ok_event {};
    struct finished_event {};
    struct publish_event {};
    struct forward_event {};
    struct ping_event {};
    struct expired_event {};

        struct terminate
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::terminate" << endl;
            }
        };        
    struct bzg_frontend : public msm::front::state_machine_def<bzg_frontend>
    {
        // states
        struct Start_tag {};
        typedef msm::front::euml::func_state<Start_tag> Start;         

        struct HaveTuple_tag {};
        typedef msm::front::euml::func_state<HaveTuple_tag> HaveTuple;         

        struct Connected_tag {};
        typedef msm::front::euml::func_state<Connected_tag> Connected;         

        struct Stop_tag {};
        typedef msm::front::euml::func_state<Stop_tag> Stop;         
        
        typedef Start initial_state;

        // actions
        struct get_first_tuple
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::get_first_tuple" << endl;
            }
        };        
        struct get_next_tuple
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::get_next_tuple" << endl;
            }
        };        
        struct store_tuple_if_new
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::store_tuple_if_new" << endl;
            }
        };        
        struct get_tuple_to_forward
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::get_tuple_to_forward" << endl;
            }
        };        
        struct send_message
        {
            template <class EVT,class FSM,class SourceState,class TargetState>
            void operator()(EVT const& ,FSM& ,SourceState& ,TargetState& )
            {
                cout << "bzg::send_message" << endl;
            }
        };        


        typedef bzg_frontend f;
        struct transition_table : mpl::vector<
            //    Start     Event         Next      Action                     Guard
            //  +---------+-------------+---------+---------------------------+----------------------+
            Row < Start   , hello_event   , HaveTuple , get_first_tuple, none> ,
            Row <HaveTuple, ok_event      , none, get_next_tuple, none>,
            Row <HaveTuple, finished_event, Connected>,
            Row <Connected, publish_event,  none, store_tuple_if_new, none>,
            Row <Connected, forward_event,  none, get_tuple_to_forward, none>,
            Row <Connected, ping_event,     none, send_message, none>,
            Row <Connected, expired_event,  Stop, terminate, none>
            > {};

        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const& e, FSM&,int state) {
            std::cout << "no transition from state " << state
                      << " on event " << typeid(e).name() << std::endl;
        }
    };
}
// Pick a back-end
typedef msm::back::state_machine<bzg_frontend> bzg_fsm;

static char const* const state_names[] = { "Start", "HaveTuple", "Connected", "Stop" };
void pstate(bzg_fsm const& bf)
{
    std::cout << " -> " << state_names[bf.current_state()[0]] << std::endl;
}


int main()
{
    bzg_fsm bf;
    bf.start();
    bf.process_event(hello_event());
    pstate(bf);
    bf.process_event(ok_event());
    pstate(bf);
    bf.process_event(finished_event());
    pstate(bf);
    bf.process_event(publish_event());
    pstate(bf);
    bf.process_event(publish_event());
    pstate(bf);
    bf.process_event(forward_event());
    pstate(bf);
    bf.process_event(ping_event());
    pstate(bf);
    bf.process_event(expired_event());
    pstate(bf);
    return 0;
}
