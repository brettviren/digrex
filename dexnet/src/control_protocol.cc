// WARNING: This is prototype code which will be invalidated.

#include "dexnet/protocols/control.h"
#include "dexnet/protohelpers.h"
#include "dexnet/node.h"
#include "dexnet/port.h"

#include "pb/testcontrol.pb.h"
#include <czmq.h>
#include <boost/sml.hpp>
#include <cxxabi.h>


namespace sml = boost::sml;

namespace dn = dexnet::node;
namespace dh = dexnet::helpers;
namespace dnc = dexnet::node::control;

namespace dexnet {
    namespace node {
        namespace protocol {
        

            int get_frame(zmsg_t* msg, int frame_index, ::google::protobuf::Message& pb)
            {
                if (!msg) {
                    zsys_error("dnp: no message");
                    return -1;
                }
                zframe_t* frame = zmsg_first(msg);
                if (!frame) {
                    zsys_error("dnp: failed to get first frame");
                    return -1;
                }
                while (frame_index) {
                    frame = zmsg_next(msg);
                    if (!frame) {
                        zsys_error("dnp: failed to get frame %d", frame_index);
                        return -1;
                    }
                    --frame_index;
                }
                bool ok = pb.ParseFromArray(zframe_data(frame), zframe_size(frame));
                if (ok) return 0;
                zsys_error("dnp: PB failed to parse");
                return -1;
            }
                

            int get_header(zmsg_t* msg, ::google::protobuf::Message& pb)
            {
                return get_frame(msg, 0, pb);
            }

            int get_payload(zmsg_t* msg, ::google::protobuf::Message& pb)
            {
                return get_frame(msg, 1, pb);
            }

            int add_payload(zmsg_t* msg, ::google::protobuf::Message& pb)
            {
                if (!msg) {
                    zsys_error("null msg");
                    return -1;
                }
                zframe_t* frame = zframe_new(NULL, pb.ByteSize());
                bool ok = pb.SerializeToArray(zframe_data(frame), zframe_size(frame));
                if (!ok) {
                    zsys_error("failed to serialize");
                    return -1;
                }
                zmsg_append(msg, &frame);
                return 0;
            }

            zmsg_t* start_msg(dn::Port* port, ::google::protobuf::Message& obj) {
                const int pcid = 0;     // fixme
                dnc::Header header;
                header.set_pcid(pcid);
                header.set_msgid(dh::msg_id(obj));
                zmsg_t* msg = port->create();
                int rc;
                rc = add_payload(msg, header);
                if (rc<0) return nullptr; // fixme cleanup msg
                rc = add_payload(msg, obj);
                if (rc<0) return nullptr; // fixme cleanup msg
                return msg;
            }
        }
    }
}
namespace dnp = dexnet::node::protocol;

template<class PBMsg>
struct evbase {
    dn::Node* node;
    dn::Port* port;
};

// these shall be codegen'ed.
// events.
struct evConnect : public evbase<dnc::Connect> {};
struct evStatus : public evbase<dnc::Status> {};
// these are "standard"
struct evOK {};
struct evFail {};
struct evUnhandled {};
//
// These above will all be codegen'ed.  


/////// finally, actions are likely hand written and are the heart
auto actConnect = [](const auto& evin, auto& sm, auto& deps, auto& subs) {
    zsys_debug("connect action");
    sm.process_event(evOK{}, deps, subs);
};
auto actStatus = [](const auto& evin, auto& sm, auto& deps, auto& subs) {
    zsys_debug("status action");
    dnc::State st;
    st.set_nodename(evin.node->name());
    st.set_payload(evin.node->payload()->name());
    dn::PortSet& ps = evin.node->ports();
    for (int pid : ps.pids()) {
        dn::Port* p = ps.get(pid);
        auto pp = st.add_states();
        pp->set_portid(pid);
        pp->set_portname(p->name());
        // fixme: there's more to transfer but leave it for now.
    }
    auto* pactor = ps.find("actor");
    dnp::start_msg(pactor, st);
    int rc = pactor->send();
    if (rc != 0) {
        zsys_error("failed to send status");
        sm.process_event(evFail{}, deps, subs);
        return;
    }
    sm.process_event(evOK{}, deps, subs);
};
/////////////////////////


struct fsm_maker {
    auto operator()() const noexcept {
        using namespace boost::sml;
        return make_transition_table (
            * "start"_s + event<evConnect> / actConnect = "decide"_s
            , "start"_s + event<evStatus> / actStatus = "decide"_s
            , "decide"_s + event<evOK> = X
            , "decide"_s + event<evFail> = "failed"_s
            , "decide"_s + event<evUnhandled> = "unhandled"_s
            );
    }
};

// 0=handled, -1=error, 1=unhandled
int dnc::Protocol::handle(Node* node, Port* pd)
{
    using namespace sml;

    zmsg_t* msg = pd->msg();
    assert(msg);

    // Here we start to depend on the message type and its schema.
    // The goal is to convert the numberical type into a C++ type.
    // This part is subject to codegen.
    // But here it will be laborious and exhausting.

    // The first part is to unpack the "header" frame to access
    // protocol and message IDs.
    dnc::Header header;
    int rc = dnp::get_header(msg, header);
    if (rc < 0) {
        zsys_error("control: failed to get header");
        return -1;
    }

    // when codegen'ed this test will be meaningful as pcid will be litteral.
    const int pcid = header.pcid();
    assert(pcid == header.pcid());

    typedef sml::sm<fsm_maker> FSM_t;
    FSM_t fsm{*this};

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
dnc::Protocol::~Protocol()
{
}


extern "C" {
    void* dexnet_protocol_factory_control() { 
        static auto myfact = new dn::ProtocolFactoryTyped<dnc::Protocol>;
        return myfact;
    }
}
