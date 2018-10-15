#include "fsm_msm_func.h"
#include "fsm_msm_func_tests.h"

//
// Custom bits MUST be provide IFF there is not body in the Jsonnet
//

// custom event
byebye::byebye(std::string name): name(name){
    std::cout << "byebye event\n";
}

// custom state enter/exit action
template <class Event, class FSM, class STATE>
void State_Exit::operator()(Event const&, FSM&, STATE& )
{
    std::cout << "Exiting state!\n";
}


// custom transition action
template <class Event, class FSM, class SrcState, class TgtState>
void Do_Something::operator()(Event const& , FSM& , SrcState& , TgtState& )
{
    std::cout << "Doing something!\n";
}



int main()
{
    std::cout << "Testing \n";
    fsm_msm_func_test();
    return 0;
}
