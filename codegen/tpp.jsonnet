// trigger primitive processor FSM

local fsm = import "fsm.jsonnet";
local n = fsm.name;


// events

local ev_connectionRequest = fsm.event("connectionRequest");


// states

local st_start = fsm.state("Start");
local st_idle = fsm.state("Idle");
local st_connecting = fsm.state("Connecting");

// actions

local ac_initialize = fsm.action("initialize");
local ac_connect = fsm.action("connect");

local ma_tpp = fsm.state("TPProcessing") {
    type: 'machine',
    initial: n(st_start),
    table: [
        {
            source: st_start,
            target: st_idle,
            action: ac_initialize,
        },
    ],
    states: fsm.machine_states(self),
    actions: fsm.machine_actions(self),
};
{
    model_name: "tpp",
    machine: ma_tpp,
}
