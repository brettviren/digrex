// Implement flow protocols

#include "czmqpb.h"

#include "dexnet/node.h"
#include "dexnet/port.h"
#include "dexnet/protocol.h"
#include "pb/testflow.pb.h"

#include <iostream>

namespace dn = dexnet::node;
namespace dnf = dexnet::node::flow;
namespace dh = dexnet::czmqpb;

class FlowSource : public dexnet::node::Protocol {
    dnf::Header m_head;
    dnf::DataTime m_dtime;
    dnf::TCSBlock m_dblock;
public:
    FlowSource () {
        m_head.set_pcid(2);     // fixme: this needs to get controlled
        // fixme: this represents concept shear!  Need to make a
        // distinction between protocol message id and protobuf's
        // message ID.
        m_head.set_msgid(dh::msg_id<dnf::TCSBlock>());

        m_dtime.set_epochns(0);

        m_dblock.set_relstartns(0);
        m_dblock.set_block_index_t(0);
        m_dblock.set_block_index_ch(0);
        m_dblock.set_noverlap_t(0);
        m_dblock.set_noverlap_ch(0);
        m_dblock.set_nbits(32);
    }

    virtual std::string name() { return "flow_source"; }

    // fixme: source has no input but needs a way to provide output
    // via timer.  Needs work in base Protocol
    virtual int handle(dn::Node* node, dn::Port* pd) {
        zsys_error("FlowSource: got input");
        return -1;
    }
    virtual int timer(dn::Node* node, int timer_id) {

        auto last_time = m_dtime.epochns();
        auto this_time = zclock_usecs() * 1000;
        m_dtime.set_epochns(this_time);

        m_dblock.set_durationns(last_time ? this_time-last_time : 0);
        m_dblock.set_block_index_t(1 + m_dblock.block_index_t());
        
        // fixme, this name is provided by configuration, need a more
        // static way to locate.
        dn::Port* p = node->ports().find("out");
        zmsg_t* msg = p->create();
        dh::append_frame(msg, m_head);
        dh::append_frame(msg, m_dtime);
        dh::append_frame(msg, m_dblock);
        int rc = p->send();
        assert(rc >= 0);

        zsys_debug("send: %d %jd %d",
                   m_dblock.block_index_t(),
                   m_dtime.epochns(),
                   m_dblock.durationns());
        return 0;
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
        zsys_debug("FlowSink sinking");
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




