local zmq = import "zmq.jsonnet";
local node = import "node.jsonnet";

//local address = "tcp://127.0.0.1:12345";
local address = "ipc:///tmp/test-three";

local source = {
    nodes: [
        node.actor(node.protocol("flowsource", "flow_source",
                                 timers=[node.timer(1)]),
                   [ node.port("out", zmq.pair, bind=address) ]),
    ],
};
local sink = {
    nodes: [
        node.actor(node.protocol("flowsink", "flow_sink"),
                   [ node.port("in", zmq.pair, connect=address) ])
    ],
};

{
    "test_source.json" : source,
    "test_sink.json": sink,
}

