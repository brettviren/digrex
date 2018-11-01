// example initialization

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
};


local readers = [
    {
        type: 'TheFactoryTypeOfReader',
        name: 'aaa',
        data: {                 // initialization data.
        }
        
    },
];
local payloads = [
    {
        type: "TheFactoryTypeOfPaylad",
        name: "fff",
        data: {
        },
    },
];

local fulldata_endpoint = "inproc://fulldata";
local slicedata_endpoint = "inproc://slices";

local filesrc = {
    type: "dfilesrc",           // this has to map to some C++ type via factory....
    name: "filesrc",

    ports: [
        /// by default the special "actor" port gets a protocol of the
        /// same name.  one can add additional protocols by naming
        /// them.  bind and type args are ignored.
        // {
        //     name: 'actor',
        //     protocols: ["myactorproto"],
        // },
        {
            name: 'output',
            type: zmq.socket_types.pair,
            bind: [fulldata_endpoint],
            protocols: ["fulldata"],
        },
    ],
};

local pusher = {
    name : "dpusher",
    type: 'PusherNodePayload',  // factory type to make a pusher payload
    controllers: ['node_controller', 'pusher_payload'], // factory types for actor pipe handling
    ports : [                   
        {
            name: 'input',
            type: zmq.socket_types.pair,
            connect: [fulldata_endpoint],
            readers: ['fulldata_reader'], // factory types for handling fulldata protocol
        },
        {
            name: 'output',
            type: zmq.socket_types.push,
            bind: [slicedata_endpoint],
            readers: ['nobacktalk'], // factory reader for handling unexpected backtalk
        },
    ],
};

local puller = function(n) {
    type: "dpusher",
    name: "puller%d" %n,
    controllers: ['node_controller', 'puller_payload'], // factory types for actor pipe handling
    ports : [
        {
            name: 'input',
            type: zmq.socket_types.pull,
            connect: [slicedata_endpoint],
            readers: ['slicedata_reader'], // factory reader for handling slice protocol
        },
    ],
};
local pullers = [puller(n) for n in std.range(0,3)];

// jsonnet -m <dir> node.jsonnet
{
    // each is an application-specific format but kept as common as possible.
    "filesrc.json": [filesrc],
    "pusher.json": [pusher],
    "puller.json": pullers,
}

