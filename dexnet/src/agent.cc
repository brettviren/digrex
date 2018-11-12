/*
  An agent represents a number of actors.

  Yes, I went there.

*/
#include "czmqpb.h"
#include "zyre.h"

#include "dexnet/node.h"
#include "dexnet/port.h"
#include "dexnet/protocol.h"
#include "pb/testcontrol.pb.h"
#include "json.hpp"

using json = nlohmann::json;
namespace dn = dexnet::node;
namespace dnc = dexnet::node::control;
namespace dh = dexnet::czmqpb;

class ZyreAgent : public dexnet::node::Protocol {
    std::string m_name;
    std::vector<zactor_t*> m_actors; // node actors
    zyre_t* m_zyre;

public:
    virtual ~ZyreAgent() {}
    virtual void configure(dn::Node* node, const std::string& name, const std::string& args) {
        m_name = name;
        m_zyre = zyre_new(m_name.c_str());
        // fixme: set headers based on which actors we were give
        // fixme: join groups baesd on args
        dn::PortSet& ps = node->ports();
        auto zyre_port = ps.add("zyre", zyre_socket(m_zyre));

        auto jcfg = json::parse(args);
        for (auto jnode : jcfg["nodes"]) {
            std::string jtext = jnode.dump();
            zactor_t* actor = zactor_new(dn::actor, (void*)jtext.c_str());
            assert(actor);
            m_actors.push_back(actor);
            ps.add(jnode["name"], zactor_sock(actor));
        }
    }

    virtual int handle(dn::Node* node, dn::Port* pd) {
        // this needs thought and may need fsm
        return 0;
    }

};

extern "C" {
    void* dexnet_protocol_factory_zyre_agent() { 
        static auto myfact = new dn::ProtocolFactoryTyped<ZyreAgent>;
        return myfact;
    }
}
