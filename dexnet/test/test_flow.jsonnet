// port: name/socktype/bind/connect/protocols
local zmq = {
    pair: 0,
    pub: 1,
    sub: 2,
    req: 3,
    rep: 4,
    dealer: 5,
    router: 6,
    pull: 7,
    push: 8,
    xpub: 9,
    xsub: 10,
    stream: 11,
};

local one = {
   "name": "testflow",
   "ports": [ ],
   "type": "control_nodeside",
   "plugins": [ { "name": "dexnet" } ]
};

local addresses = {
    source: "inproc://source",
    splits: ["inproc://split%d"%n for n in std.range(0,3)],
};

local timer = function(delayms, ntimes=0) {
    delay: delayms, ntimes:ntimes,
};

local protocol = function(name, type, timers=[]) {
    name:name, type:type, timers:timers,
};

local port = function(name, ztype, protocols=[]) {
    name:name, type:ztype, protocols:protocols,
};

local actor = function(payload, ports=[]) {
    name: payload.name,
    payload: payload,
    ports: ports, 
    plugins: [ { "name": "dexnet" } ], // fixme: should be conifg on main(), not actors
};

local actors = [
    actor(protocol("flowsource", "flow_source", [timer(1)]),
          [ port("out", zmq.pair) ]),
    actor(protocol("flowsplit", "flow_split"),
          [ port("in", zmq.pair) ] + [port("out%d"%n, zmq.pair) for n in std.range(0,3)])
] + [
    actor(protocol("flowsink%d", "flow_sink"), [ port("in", zmq.pair) ])
    for n in std.range(0,3)
    ];
actors
