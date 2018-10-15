local fsm = import "fsm.jsonnet";
local n = fsm.name;

//
// Events
//

local hello_event = {
    name: "hello",
    attrs: [
        { name: "name", type: "std::string", },
        { name: "data", type: "std::vector<int>", }
    ],

    body: |||
 // This is a generated body
 // modifications will be lost.
 std::cout << "hello event created" << std::endl;        
|||
};
local byebye_event = {
    name: "byebye",
    attrs: [ {name: "name", type: "std::string"} ],
};



//
// Enter/Exist actions
//
local enter_action = {
    name: "State_Enter",
    body: 'std::cout << "Entering State" << std::endl;'
};
local exit_action = {
    name: "State_Exit",
//    body: 'std::cout << "Exiting State" << std::endl;'
};


//
// Transition actions
//

local do_something = {
    name: "Do_Something",
//    body: 'std::cout << "Something" << std::endl;'
};


//
// States
//

local start_state = {
    name: "Start",
    enter: n(enter_action),
    exit: n(exit_action),
};
local stop_state = start_state { name: "Stop" };


// 
// Machines
//
local example_machine = start_state{       // also may be a state
    name: "ExampleFSM",
    initial: n(start_state),
    table: [
        {
            source: n(start_state),
            event: n(hello_event),   // optional, omit for direct
            //target: "",              // optional, omit for internal
            action: n(do_something), // optional
            //guard: "",              // optional, omit
        },
        {
            source: n(start_state),
            event: n(byebye_event),
            target: n(stop_state),
            action: n(do_something),
        }
    ],
    states: fsm.machine_states(self),

};


//
// The system
//

local example_main = {
    system_name: std.extVar("system"),
    model_name: "fsm_test",
    states: [start_state, stop_state],
    enter_exits: [enter_action, exit_action],
    transitions: [do_something],
    events: [hello_event, byebye_event],
    machines: [example_machine],
    test_chain: [
        {
            name: n(hello_event),
            args: '"hello", {}'
        },
        {
            name: n(byebye_event),
            args: '"byebye"'
        }
    ],
};
example_main
