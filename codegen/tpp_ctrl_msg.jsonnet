
local msg_connect = {
    name: "connect",
    event: "connectRequest",    // an event to match
    desc: "Request a connection to an endpoint",
    fields: [
        {
            name: "endpoint",
            type: "string",
            desc: "URL of endpoint to connect",
        },
    ],
};

[
    msg_connect,
]
