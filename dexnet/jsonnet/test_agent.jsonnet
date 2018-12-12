// Define configuration for a test agent node.
//
// A agent node is one which manages actor nodes.  Yes.
//
// An agent supports publicizing (discovery/presence) its actors (via
// Zyre) and as a middleman for those which wish to control the
// actors.

local zmq = import "zmq.jsonnet";
local node = import "node.jsonnet";
local actors = import "test_flow.jsonnet";



// payload_protocol, ports
{
    plugins: [ { "name": "dexnet" } ],
    nodes: [
        node.actor(node.protocol("testagent", "zyre_agent", {nodes:actors}))
    ],
}


