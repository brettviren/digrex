// Define configuration for an agent node.
//
// A agent node is one which manages actor nodes.  Yes.
//
// An agent supports publicizing (discovery/presence) its actors (via
// Zyre) and as a middleman for those which wish to control the
// actors.

local zmq = import "zmq.jsonnet";
local actors = import "test_flow.jsonnet";


