#include "dexnet/protocol.h"
#include <czmq.h>

namespace dn = dexnet::node;

dn::ProtocolFactory* dn::protocol_factory(upif::cache& plugins,
                                          const std::string& protocol_typename)
{
    const std::string symname = "dexnet_protocol_factory_" + protocol_typename;
    upif::plugin* plugin = plugins.find(symname);
    if (!plugin) {
        zsys_error("no factory for \"%s\"", symname.c_str());
        return nullptr;
    }
    
    dn::factory_maker fm = nullptr;
    bool ok = plugin->symbol<dn::factory_maker>(symname, fm);
    void* vpf = (*fm)();
    if (ok) {
        return static_cast<dn::ProtocolFactory*>(vpf);
    }
    return nullptr;
}

dn::ProtocolFactory* dn::protocol_factory(const std::string& protocol_typename)
{
    auto& c = upif::singleton<upif::cache>::instance();
    return dn::protocol_factory(c, protocol_typename);
}



dn::Protocol::~Protocol()
{
}

