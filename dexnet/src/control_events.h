#ifndef dexnet_node_control_events_h_seen
#define dexnet_node_control_events_h_seen

#include "dexnet/node.h"
#include "dexnet/port.h"

#include "pb/testcontrol.pb.h"

template<class PBMsg>
struct evbase {
    dexnet::node::Node* node;
    dexnet::node::Port* port;

    // Here put get/set of port msg using pb objects

};

// these shall be codegen'ed.
// events, one for each pb.
struct evConnect : public evbase<dexnet::node::control::Connect> {};
struct evStatus : public evbase<dexnet::node::control::Status> {};
// these are "standard" events
struct evOK {};
struct evFail {};
struct evUnhandled {};

#endif
