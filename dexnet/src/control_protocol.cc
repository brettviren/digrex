// WARNING: This is prototype code which will be invalidated once moved to codegen.

#include "czmqpb.h"

#include "dexnet/node.h"
#include "dexnet/port.h"
#include "dexnet/protocol.h"
#include "pb/testcontrol.pb.h"

#include "control_events.h"
#include "control_actions.h"

#include <czmq.h>
#include <cxxabi.h>

#include <boost/sml.hpp>

#include <queue>

namespace dn = dexnet::node;
namespace dnc = dexnet::node::control;
namespace dh = dexnet::czmqpb;


struct fsm_maker {
    auto operator()() const noexcept {
        using namespace boost::sml;
        return make_transition_table (
            * "start"_s + event<evConnect> / dnc::actConnect{} = "decide"_s
            , "start"_s + event<evStatus> / dnc::actStatus{} = "decide"_s
            , "decide"_s + event<evOK> = X
            , "decide"_s + event<evFail> = "failed"_s
            , "decide"_s + event<evUnhandled> = "unhandled"_s
            );
    }
};
 
// fixme: also need PatronSide
class NodeSide : public dexnet::node::Protocol {
public:
    virtual ~NodeSide() {}
    
    virtual std::string name() { return "control"; }
    
    // 0=handled, -1=error, 1=unhandled
    virtual int handle(dn::Node* node, dn::Port* pd) {

        using namespace boost::sml;

        zmsg_t* msg = pd->msg();
        assert(msg);

        // Here we start to depend on the message type and its schema.
        // The goal is to convert the numberical type into a C++ type.
        // This part is subject to codegen.
        // But here it will be laborious and exhausting.

        // The first part is to unpack the "header" frame to access
        // protocol and message IDs.
        dnc::Header header;
        int rc = dh::get_frame(msg, 0, header);
        if (rc < 0) {
            zsys_error("control: failed to get header");
            return -1;
        }

        // when codegen'ed this test will be meaningful as pcid will be litteral.
        const int pcid = header.pcid();
        assert(pcid == header.pcid());

        sm<fsm_maker, process_queue<std::queue> > fsm{*this};

        // fixme: finally we upcast to C++ type by wrapping the message to
        // a typed unpacker.  These unpackers shall be defined above but
        // really must come from codegen.  This long switch also shall be
        // from codegen.
        const int msgid = header.msgid();
        switch (msgid) {
        default: return 1; break;

            // fixme: case shall use an enum defined bythe message codegen.
        case 1:
            // fixme: evX structs shall be named, not numbered, by codegen
            fsm.process_event(evConnect{node,pd});
            break;
        case 2:
            fsm.process_event(evStatus{node,pd});
            break;
        }
    
        if (fsm.is(X)) {
            zsys_error("dnc: FSM succeeded");
            return 0;
        }
        if (fsm.is("failed"_s)) {
            zsys_error("dnc: FSM failed");
            return -1;
        }
        if (fsm.is("unhandled"_s)) {
            zsys_error("dnc: FSM unhandled");
            return 1;
        }

        // Fixme: look into using exceptions inside the FSM.
        zsys_error("dnc: FSM landed in error");
        return -1;                  // error
    }
};


extern "C" {
    void* dexnet_protocol_factory_control_nodeside() { 
        static auto myfact = new dn::ProtocolFactoryTyped<NodeSide>;
        return myfact;
    }
}




