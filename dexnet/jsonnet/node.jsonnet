// Some functions to standardize data structures related to
// configuring nodes.

{
    timer :: function(delayms, ntimes=0) {
        delay: delayms, ntimes:ntimes,
    },

    protocol :: function(name, type, args={}, timers=[]) {
        name:name, type:type, timers:timers, args:args
    },

    port :: function(name, ztype, protocols=[], bind=[], connect=[]) {
        name:name, type:ztype, protocols:protocols,
        bind:bind, connect:connect
    },

    actor :: function(payload, ports=[]) {
        name: payload.name,
        payload: payload,
        ports: ports, 
    },
}
