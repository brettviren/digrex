#include "dexnet/protocol.h"

namespace dn = dexnet::node;


dn::ProtocolFactory* dn::protocol_factory(upif::cache& plugins,
                                          const std::string& protocol_typename)
{
    const std::string symname = "dexnet_protocol_factory_" + protocol_typename;
    upif::plugin* plugin = plugins.find(symname);
    dn::ProtocolFactory* factory=nullptr;    
    bool ok = plugin->symbol<dn::ProtocolFactory*>(symname, factory);
    if (ok) {
        return factory;
    }
    return nullptr;
}
