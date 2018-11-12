#include "dexnet/node.h"
#include "dexnet/protocol.h"
#include "dexnet/protohelpers.h"

#include <czmq.h>

using json = nlohmann::json;
namespace dn = dexnet::node;
namespace dh = dexnet::helpers;

const std::string actor_port_name = "actor";
const std::string actor_ctrl_proto = "control_nodeside";



dn::Node::Node(zsock_t* pipe, void* args)
{
    m_loop = zloop_new();
    auto port = m_ports.add(actor_port_name, pipe);
    assert(port);
    assert(args);
    initialize((char*)args);
}
dn::Node::~Node()
{
}
static int handle_timer(zloop_t* loop, int timer_id, void* vobj)
{
    dn::Node* node = (dn::Node*)vobj;
    return node->timer(timer_id);
}

void dn::Node::initialize_protocol(json& jcfg, const std::string& portname)
{
    std::string pctype = jcfg["type"];
    std::string pcname = jcfg["name"];
    if (portname == "") {
        zsys_debug("adding payload \"%s\"", pctype.c_str());
    }
    else {
        zsys_debug("adding protocol \"%s\" to port \"%s\"",
                   pctype.c_str(), portname.c_str());
    }
    auto pfact = dn::protocol_factory(m_plugins, pctype); 
    assert(pfact);

    auto proto = pfact->create(pcname);
    assert(proto);
    proto->configure(this, pcname, jcfg["args"].dump());

    if (portname.empty()) {
        m_payload = proto;
    }
    else {
        auto port = m_ports.find(portname);
        assert(port);
        port->add(proto);
    }

    for (auto jtimer : jcfg["timers"]) {
        const int delay = jtimer["delay"];
        const int ntimes = jtimer["ntimes"];
        int timer_id = zloop_timer(m_loop, delay, ntimes, handle_timer, this);
        m_timers[timer_id] = proto;
    }
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

    
    initialize_protocol(jcfg["payload"]);

    {                 // hard-wire the actor control protocol and port
        json jctrl;
        jctrl["type"] = actor_ctrl_proto;
        initialize_protocol(jctrl, actor_port_name);
    }

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
            zsys_debug("node: bind port \"%s\" to \"%s\"",
                       port->name().c_str(), ep.c_str());
            int rc = port->bind(ep);
            assert(rc >= 0);    // port number is returned in a bind.
        }
        for (auto jep : jp["connect"]) {
            std::string ep = jep;
            zsys_debug("node: connect port \"%s\" to \"%s\"",
                       port->name().c_str(), ep.c_str());
            int rc = port->connect(ep);
            assert(rc == 0);
        }
        for (auto jpc : jp["protocols"]) {
            initialize_protocol(jpc, portname);
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

int dn::Node::timer(int timer_id)
{
    auto it = m_timers.find(timer_id);
    if (it == m_timers.end()) {
        zsys_warning("Node: \"%s\" unknown timer ID: %d", m_name.c_str(), timer_id);
        return 0;
    }
    dn::Protocol* pc = it->second;
    return pc->timer(this, timer_id);
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
    zsys_debug("Node actor \"%s\" ready", node.name().c_str());
    zsock_signal(pipe, 0);      // ready
    node.run();
    // node destructor destroys ports which destroys sockets
}
