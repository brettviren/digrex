local name = function(obj) obj["name"];

local start_state = {
    name: "Start",
    enter: "State_Enter",        // an action
    exit: "State_Exit",          // an action
};
local stop_state = {
    name: "Stop",
    enter: "State_Enter",        // an action
    exit: "State_Exit",          // an action
};
local enter_action = {
    name: "State_Enter",
    // optional body text in target language.
    //return: "void",             // optional, bool for guards, default is void
    body: {
        msm_euml: |||
 
 std::cout << "Entering State" << std::endl;
|||,
    }
};
local exit_action = {
    name: "State_Exit",
    body: {
        msm_euml: |||
 
 std::cout << "Exiting State" << std::endl;
|||,
    }
};
local do_something = {
    name: "Do_Something",
    body: {
        msm_euml: |||
 
 std::cout << "Something" << std::endl;
|||,
    }
};
local hello_event = {
    name: "hello",
    attrs: [
        {
            name: "name",
            type: "std::string",
        },
        {
            name: "data",
            type: "std::vector<int>",
        }
    ],        
};
local byebye_event = {
    name: "byebye",
};
local example_machine = {       // also may be a state
    name: "ExampleFSM",
    enter: name(enter_action),
    exit:  name(exit_action),
    //attrs : [],
    initial: name(start_state),
    table: [
        {
            source: name(start_state),
            event: name(hello_event),   // optional, omit for direct
            //target: "",              // optional, omit for internal
            action: name(do_something), // optional
            //guard: "",              // optional, omit
        },
        {
            source: name(start_state),
            event: name(byebye_event),
            target: name(stop_state),
            action: name(do_something),
        }
    ],
};
local example_main = {
    states: [start_state, stop_state],
    enter_exits: [enter_action, exit_action],
    transitions: [do_something],
    events: [hello_event, byebye_event],
    machines: [example_machine],
    test_chain: [ name(hello_event), name(byebye_event) ],
};
example_main
