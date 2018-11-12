// port: name/socktype/bind/connect/protocols
local zmq = import "zmq.jsonnet";
local node = import "node.jsonnet";


local addresses = {
    source: "inproc://source",
    splits: ["inproc://split%d"%n for n in std.range(0,3)],
};


local actors = [
    node.actor(node.protocol("flowsource", "flow_source", timers=[node.timer(1)]),
          [ node.port("out", zmq.pair) ]),
    node.actor(node.protocol("flowsplit", "flow_split"),
          [ node.port("in", zmq.pair) ] + [node.port("out%d"%n, zmq.pair) for n in std.range(0,3)])
] + [
    node.actor(node.protocol("flowsink%d"%n, "flow_sink"), [ node.port("in", zmq.pair) ])
    for n in std.range(0,3)
    ];
actors
