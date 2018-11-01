#include "dexnet/port.h"
#include "dexnet/protocol.h"
#include <numeric>

namespace dn = dexnet::node;


dn::Port::Port(portid_t id, std::string name, zsock_t* sock, bool owned)
    : m_id(id), m_name(name), m_sock(sock), m_socket_owned(owned)
{
}
dn::Port::~Port()
{
    if (m_socket_owned) {
        zsock_destroy(&m_sock);
    }
}

// bind a port's socket and remember endpoint.  ZMQ RC value returned.
int dn::Port::bind(const std::string& endpoint)
{
    int portnum = zsock_bind(m_sock, endpoint.c_str(), NULL);
    if (portnum < 0) {
        return portnum;
    }
    m_bound[endpoint] = portnum;
    return portnum;
}
// connect a port's socket and remember endpoint.  ZMQ RC value returned.
int dn::Port::connect(const std::string& endpoint)
{
    int rc = zsock_connect(m_sock, endpoint.c_str(), NULL);
    if (rc < 0) {
        return rc;
    }
    m_connected.insert(endpoint);
    return rc;
}

void dn::Port::add(dn::Protocol* proto)
{
    m_protos.push_back(proto);
}
int dn::Port::handle(dn::Node* node)
{
    int rch=0;
    for (auto proto : m_protos) {
        rch = proto->handle(node, this);
        // error or handled
        if (rch <= 0) { return rch; }
    }
    return 1;                   // not handled.
}


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
