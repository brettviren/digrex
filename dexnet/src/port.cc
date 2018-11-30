#include "dexnet/port.h"
#include "dexnet/protocol.h"
namespace dn = dexnet::node;


dn::Port::Port(portid_t id, std::string name, zsock_t* sock, bool owned)
    : m_id(id), m_name(name), m_sock(sock), m_msg(nullptr), m_socket_owned(owned)
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
        zsys_error("port: failed to bind to %s", endpoint.c_str());
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
        zsys_error("port: failed to connect to %s", endpoint.c_str());
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

zmsg_t* dn::Port::recv()
{
    if (m_msg) {
        zmsg_destroy(&m_msg);
    }
    m_msg = zmsg_recv(m_sock);
    zsys_debug("port %s recv %d/%d on 0x%x", name().c_str(),
               zmsg_content_size(m_msg), zmsg_size(m_msg), m_sock);
    return m_msg;
}

int dn::Port::send()
{
    if (!m_msg) {
        return -1;
    }
    zsys_debug("port %s send %d/%d on 0x%x", name().c_str(),
               zmsg_content_size(m_msg), zmsg_size(m_msg), m_sock);
    return zmsg_send(&m_msg, m_sock); // clears msg
}

zmsg_t* dn::Port::msg()
{
    if (!m_msg) {
        m_msg = zmsg_new();
    }
    return m_msg;
}

zmsg_t* dn::Port::create()
{
    if (m_msg) {
        zmsg_destroy(&m_msg);
    }
    m_msg = zmsg_new();
    return m_msg;
}




