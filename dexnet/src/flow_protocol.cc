// Implement flow protocols

#include "czmqpb.h"

#include "dexnet/node.h"
#include "dexnet/port.h"
#include "dexnet/protocol.h"
#include "pb/testflow.pb.h"

namespace dn = dexnet::node;
namespace dnf = dexnet::node::flow;
namespace dh = dexnet::czmqpb;

class FlowSource : public dexnet::node::Protocol {
public:
    virtual std::string name() { return "flow_source"; }

    // fixme: source has no input but needs a way to provide output
    // via timer.  Needs work in base Protocol
    virtual int handle(dn::Node* node, dn::Port* pd) {
        zsys_error("FlowSource: got input");
        return -1;
    }
};
class FlowSplit : public dexnet::node::Protocol {
public:
    virtual std::string name() { return "flow_split"; }
    virtual int handle(dn::Node* node, dn::Port* pd) {

        zmsg_t* msg = pd->msg();
        assert(msg);

        // fixme: check message is expected proto/msg type

        int rc;

        dnf::Header header;
        rc = dh::get_frame(msg, 0, header);
        assert (rc == 0);

        dnf::DataTime dtime;
        rc = dh::get_frame(msg, 1, dtime);
        assert (rc == 0);

        dnf::TCSBlock dblock;
        rc = dh::get_frame(msg, 2, dblock);
        assert (rc == 0);
        
        // fixme: split according to configuration, which must match
        // port definitions.  For now cheat.
        const int nsplit = 4;

        // Here we'd do the actual split but cheat more by simply
        // giving each output the full block.

        auto& ps = node->ports();
        for (int isplit=0; isplit < nsplit; ++isplit) {
            // fixme: number->port mapping should be handled once in
            // configuration() method which doesn't yet exist.
            std::string pname = "out";
            pname += '0' + isplit;
            dn::Port* outp = ps.find(pname);
            assert(outp);
            zmsg_t* outmsg = outp->create();
            // fixme: again, we are cheating here.
            rc = dh::append_frame(outmsg, header);
            assert(rc == 0);
            rc = dh::append_frame(outmsg, dtime);
            assert(rc == 0);
            rc = dh::append_frame(outmsg, dblock);
            assert(rc == 0);
        }
        
        return 0;
    }

};
class FlowSink : public dexnet::node::Protocol {
public:
    virtual std::string name() { return "flow_sink"; }
    virtual int handle(dn::Node* node, dn::Port* pd) {
        return 0;
    }

};



extern "C" {
    void* dexnet_protocol_factory_flow_source() { 
        static auto myfact = new dn::ProtocolFactoryTyped<FlowSource>;
        return myfact;
    }
    void* dexnet_protocol_factory_flow_split() { 
        static auto myfact = new dn::ProtocolFactoryTyped<FlowSplit>;
        return myfact;
    }
    void* dexnet_protocol_factory_flow_sink() { 
        static auto myfact = new dn::ProtocolFactoryTyped<FlowSink>;
        return myfact;
    }
}




