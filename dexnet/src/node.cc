#include "dexnet/node.h"
#include "dexnet/protocol.h"

#include "json.hpp"
#include <czmq.h>

using json = nlohmann::json;
namespace dn = dexnet::node;

const std::string actor_port_name = "actor";
const std::string actor_ctrl_proto = "actor";

dn::Node::Node(zsock_t* pipe, void* args)
{
    auto port = m_ports.add(actor_port_name, pipe);
    assert(port);
    if (args) {
        initialize((char*)args);
    }
}
dn::Node::~Node()
{
}

void dn::Node::addproto(const std::string& protoname,
                        const std::string& portname)
{
    auto pfact = dn::protocol_factory(plugins, protoname);
    assert(pfact);
    // for now, always pass node name as protocol instance name.  Some
    // point later we may want to differently configured instances of
    // a given protocol type and then we'd use a unique instance name
    // as determined by our initialization configuration.
    auto proto = pfact->create(m_name);
    assert(proto);

    if (portname.empty()) {
        return;
    }
    auto port = m_ports.find(portname);
    assert(port);
    port->add(proto);
}

void dn::Node::initialize(const std::string& json_text)
{
    // fixme: initialize upfi, maybe make it a singleton to main() does init.

    auto jcfg = json::parse(json_text);
    m_name = jcfg["name"];

    std::string type = jcfg["type"];
    addproto(type);

    addproto(actor_ctrl_proto, actor_port_name);

    for (auto jp : jcfg["ports"]) {
        std::string portname = jp["name"];
        Port* port = nullptr;
        if (portname == actor_port_name) {
            port = m_ports.find(portname);
        }
        else {
            int socktype = jp["type"];
            port = m_ports.make(portname, socktype);
        }
        assert (port);
        for (auto ep : jp["bind"]) {
            int rc = port->bind(ep);
            assert(rc >= 0);    // port number is returned in a bind.
        }
        for (auto ep : jp["connect"]) {
            int rc = port->connect(ep);
            assert(rc == 0);
        }
        for (auto protoname : jp["protocols"]) {
            addproto(protoname, portname);
        }
    }

    
    
}


int dn::Node::input(zsock_t* sock)
{
    auto port = m_ports.find(sock);
    assert(port);

    port->recv();

    int rch = port->handle(this);
    if (rch <= 0) {             // error or handled
        return rch;
    }

    return m_payload->handle(this, port);
}


static int handle_input (zloop_t *loop, zsock_t *sock, void *vobj)
{
    dn::Node* node = static_cast<dn::Node*>(vobj);
    int rch = node->input(sock);
    if (rch <= 0) return rch;
    return -1;                  // not handling input is an error.
}


void dn::Node::run()
{
    m_loop = zloop_new();
    for (auto pid : m_ports.pids()) {
        auto port = m_ports.get(pid);
        assert(port);
        int rc = zloop_reader(m_loop, port->sock(), handle_input, this);
        assert (rc == 0);
    }
    zloop_start(m_loop);
    zloop_destroy(&m_loop);
}


// 
void dn::actor(zsock_t* pipe, void* vargs)
{
    dn::Node node(pipe, vargs);
    zsock_signal(pipe, 0);      // ready
    node.run();
    // node destructor destroys ports which destroys sockets
}
