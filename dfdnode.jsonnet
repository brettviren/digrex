/* 

A "node" data structure associates an instance of a C++ class name
(the 'type' attribute) with an instance name and a number of ports
expected by the C++ class.  It also associates a host on which to
execute and any class-specific configuration parameters.

A "port" data structure associates a name unique to the node, an
endpoint URL, a socket type and method ("bind" vs "connect").

An "edge" data structure is simply an ordered pair of ports.  


*/
local zmq = {
    // copied from zmq.h
    socket_types: {
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
    },

    socket_opts: {
        affinity: 4,
        routing_id: 5,
        subscribe: 6,
        unsubscribe: 7,
    },        

};

local dfd = {
    
    default :: {
        host: {
            name:"127.0.0.1",
            port: 9875,
        },
        url: 'tcp://%s:%d' % [self.host.name, self.host.port],
        sock: {
            type: "pub",
            meth: "bind",
        }
    },


    port :: function(name, type='pub', meth='bind',
                     url=self.default.url, sockopts=[]) {
        name:name, url:url, type:zmq.socket_types[type], meth:meth,
        sockopts: sockopts,
    },
    

    edge :: function(tail_port, head_port) {
        tail: '%s:%s' % [tail_port.parent, tail_port.name],
        head: '%s:%s' % [head_port.parent, head_port.name],
    },

    // ports is a list of strings (names).  
    node :: function(name, type, ports,
                     params={},
                     hostname=self.default.host.name)
    {
        name:name, type:type, params:params, hostname:hostname, 

        // fully qualified name
        local fqn = "%s:%s" % [self.type, self.name],

        usn :: function() "%s_%s" % [self.type, self.name],

        ports: { [p.name]:p{parent:fqn} for p in ports},

    },
};

local source = dfd.node("source42", "PASource",
                        ports = [ dfd.port('outbox', 'pub', 'bind') ],
                        params={ fragid: 42 });


local sink = dfd.node("sink", "PASink",
                      ports = [
                          dfd.port('inbox', 'sub', 'connect',
                                   sockopts=[{
                                       opt:zmq.socket_opts["subscribe"],
                                       arg:""
                                   }])]);
                      

local nodes = [source, sink];

{
    "graph.json": {
        nodes: nodes,
        edges: [
            dfd.edge(source.ports["outbox"], sink.ports["inbox"]),
        ],
    },
} + { [n.usn() + ".json"]:n for n in nodes }
