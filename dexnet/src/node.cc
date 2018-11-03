#include "dexnet/node.h"
#include "dexnet/protocol.h"
#include "dexnet/protohelpers.h"

#include "json.hpp"
#include <czmq.h>

using json = nlohmann::json;
namespace dn = dexnet::node;
namespace dh = dexnet::helpers;

const std::string actor_port_name = "actor";
const std::string actor_ctrl_proto = "control";

dn::Node::Node(zsock_t* pipe, void* args)
{
    auto port = m_ports.add(actor_port_name, pipe);
    assert(port);
    assert(args);
    initialize((char*)args);
}
dn::Node::~Node()
{
}

void dn::Node::addproto(const std::string& protoname,
                        const std::string& portname)
{
    zsys_debug("adding protocol \"%s\" to port \"%s\"",
               protoname.c_str(), portname.c_str());
    auto pfact = dn::protocol_factory(m_plugins, protoname);
    if (!pfact) {
        zsys_error("failed to get protocol: %s", protoname.c_str());
    }
    assert(pfact);

    // for now, always pass node name as protocol *instance* name.
    // Some point later we may want to differently configured
    // instances of a given protocol type and then we'd use a unique
    // instance name as determined by our initialization
    // configuration.
    auto proto = pfact->create(m_name);
    assert(proto);

    if (portname.empty()) {
        m_payload = proto;
        return;
    }
    auto port = m_ports.find(portname);
    assert(port);
    port->add(proto);
}

void dn::Node::initialize(const std::string& json_text)
{
    zsys_debug("config string:\n%s", json_text.c_str());

    auto jcfg = json::parse(json_text);
    m_name = jcfg["name"];

    // fixme: initialize upfi, maybe make it a singleton to main() does init.
    for (auto jpin : jcfg["plugins"]) {
        std::string pname = jpin["name"];
        std::string plib = "";
        if (jpin["library"].is_string()) {
            plib = jpin["library"];
        }
        zsys_debug("add plugin: %s %s", pname.c_str(), plib.c_str());
        auto pi = m_plugins.add(pname, plib);
        assert(pi);
    }

    zsys_debug("actor proto: %s, actor port: %s", actor_port_name.c_str(), actor_port_name.c_str());
    addproto(actor_ctrl_proto, actor_port_name);

    std::string type = jcfg["type"];
    addproto(type);

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
        for (auto jep : jp["bind"]) {
            std::string ep = jep;
            zsys_debug("node: bind port %s to %s",
                       port->name().c_str(), ep.c_str());
            int rc = port->bind(ep);
            assert(rc >= 0);    // port number is returned in a bind.
        }
        for (auto jep : jp["connect"]) {
            std::string ep = jep;
            zsys_debug("node: connect port %s to %s", port->name().c_str(), ep.c_str());
            int rc = port->connect(ep);
            assert(rc == 0);
        }
        for (auto protoname : jp["protocols"]) {
            addproto(protoname, portname);
        }
    }

    assert(m_payload);
    
}


void dn::Node::shutdown()
{
    for (auto pid : m_ports.pids()) {
        auto port = m_ports.get(pid);
        assert(port);
        zloop_reader_end(m_loop, port->sock());
    }
}

int dn::Node::input(zsock_t* sock)
{
    if(!sock) {
        zsys_error("Node: got null input socket");
        return -1;
    }

    auto port = m_ports.find(sock);
    if (!port) {
        zsys_error("node: failed to get port for socket");
        return -1;
    }

    auto msg = port->recv();
    if (!msg) {
        zsys_error("node: failed to recv port");
        return -1;
    }
    if (dh::msg_term(msg)) {
        zsys_debug("node: got $TERM");
        shutdown();
        return -1;
    }

    int rch = port->handle(this);
    if (rch <= 0) {             // error or handled
        zsys_debug("node: port handled: %d", rch);
        return rch;
    }

    rch = m_payload->handle(this, port);
    zsys_debug("node: payload handled: %d", rch);
    return rch;    
}


static int handle_input (zloop_t *loop, zsock_t *sock, void *vobj)
{
    dn::Node* node = static_cast<dn::Node*>(vobj);
    zsys_debug("handling input on 0x%x", (void*)sock);
    int rch = node->input(sock);
    zsys_debug("handled input: %d", rch);
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
        zsys_debug("node: adding port %s to loop", port->name().c_str());
        assert (rc == 0);
    }
    zloop_start(m_loop);
    zloop_destroy(&m_loop);
}


// 
void dn::actor(zsock_t* pipe, void* vargs)
{
    dn::Node node(pipe, vargs);
    zsys_debug("Node actor ready");
    zsock_signal(pipe, 0);      // ready
    node.run();
    // node destructor destroys ports which destroys sockets
}
