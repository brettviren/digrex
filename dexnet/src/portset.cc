#include "dexnet/portset.h"

#include <numeric>              // iota


namespace dn = dexnet::node;

dn::PortSet::PortSet()
{
}
dn::PortSet::~PortSet()
{
}

            
dn::Port* dn::PortSet::make(const std::string& name, int zmqsocktype)
{
    if (this->find(name)) {
        return nullptr;
    }
    zsock_t* sock = zsock_new(zmqsocktype);
    if (!sock) {
        return nullptr;
    }
    dn::portid_t pid = m_store.size();
    const bool owned = true;
    m_store.push_back(std::unique_ptr<Port>(new Port(pid, name, sock, owned)));
    m_indbyname[name] = pid;
    m_indbysock[sock] = pid;
    return m_store[pid].get();
}

dn::Port* dn::PortSet::add(const std::string& name, zsock_t* sock)
{
    if (this->find(name)) {
        return nullptr;
    }
    dn::portid_t pid = m_store.size();
    const bool owned = false;
    m_store.push_back(std::unique_ptr<Port>(new Port(pid, name, sock, owned)));
    m_indbyname[name] = pid;
    m_indbysock[sock] = pid;
    return m_store[pid].get();
}

dn::Port* dn::PortSet::find(const std::string& name)
{
    auto it = m_indbyname.find(name);
    if (it == m_indbyname.end()) {
        return nullptr;
    }
    return m_store[it->second].get();
}

dn::Port* dn::PortSet::find(zsock_t* sock)
{
    auto it = m_indbysock.find(sock);
    if (it == m_indbysock.end()) {
        return nullptr;
    }
    return m_store[it->second].get();
}

dn::Port* dn::PortSet::get(dn::portid_t pid)
{
    if (pid < 0 or pid >= m_store.size()) {
        return nullptr;
    }
    return m_store[pid].get();
}

dn::PortSet::pids_t dn::PortSet::pids()
{
    // In future, a port may have a lifetime less than that of the
    // ports collection.  Then the store may be sparse or may be a map
    // from pid to Port.  For now, valid pids are all indices
    // into the vector.
    pids_t ret(m_store.size(), 0);
    std::iota(ret.begin(), ret.end(), 0);
    return ret;
}

