#include "dexnet/node.h"

#include "json.hpp"
#include <czmq.h>

using json = nlohmann::json;
namespace dn = dexnet::node;

int dn::Port::bind(const std::string& endpoint)
{
    int port = zsock_bind(socket, endpoint.c_str(), NULL);
    if (port < 0) { return port; }
    bound[endpoint] = port;
    return port;
}
int dn::Port::connect(const std::string& endpoint)
{
    int rc = zsock_connect(socket, endpoint.c_str(), NULL);
    if (rc < 0) { return rc; }
    connected.insert(endpoint);
    return rc;
}


dn::PortSet::PortSet() {
    add(dn::Port{});        // add dummy at index 0
}

size_t dn::PortSet::add(const dn::Port& p)
{
    const size_t ind = m_ports.size();
    m_indbysock[p.socket] = ind;
    m_indbyname[p.name] = ind;
    m_ports.push_back(p);
    return ind;
}

dn::Port& dn::PortSet::get(const std::string& name)
{
    const size_t ind = m_indbyname[name];
    return m_ports[ind];
}

dn::Port& dn::PortSet::get(zsock_t* sock)
{
    const size_t ind = m_indbysock[sock];
    return m_ports[ind];
}

bool dn::PortSet::have(const std::string& name)
{
    return m_indbyname.find(name) != m_indbyname.end();
}
bool dn::PortSet::have(zsock_t* sock)
{
    return m_indbysock.find(sock) != m_indbysock.end();
}


dn::Node::Node(zsock_t* pipe, void* args)
{
    m_ports.add(dn::Port{"actor", pipe});
    if (args) {
        initialize((char*)args);
    }
}
dn::Node::~Node()
{
    zsock_t* avoid = m_ports.get(1).socket;

    for (auto p : m_ports.asvector()) {
        if (p.socket = avoid) {
            continue;           // skip agent pipe socket
        }
        zsock_destroy(&p.socket);
    }
}

void dn::Node::initialize(const std::string& json_text)
{
    // convert to jcfg object

    auto jcfg = json::parse(json_text);
    m_name = jcfg["name"];

    for (auto jp : jcfg["ports"]) {
        if (m_ports.have(jp["name"])) {
            zsys_error("%s: already have port %s", jp["name"]);
            continue;
        }
        zsock_t* sock = zsock_new(jp["type"].get<int>());
        Port p{jp["name"], sock};
        for (ep : jp["bind"]) {
            p.bind(ep);
        }
        for (ep : jp["connect"]) {
            auto eps = ep.get<std::string>();
            zsock_connect(sock, eps.c_str(), NULL);
            p.connected.insert(eps);
        }
    }

    //m_payload = magic_factory(jcfg.type);
}

int dn::Node::input(zsock_t* sock)
{
    // what to put here?
}


static int handle_input (zloop_t *loop, zsock_t *sock, void *vobj)
{
    dn::Node* node = static_cast<dn::Node*>(vobj);
    return node->input(sock);
}


void dn::Node::run()
{
    zloop_t* loop = zloop_new();
    for (auto p : m_ports.asvector()) {
        int rc = zloop_reader(loop, p.socket, handle_input, this);
        assert (rc == 0);
    }
    zloop_start(loop);

    zloop_destroy(&loop);
}


// fixme: put this all in a dexnet::node:: namespace
void dn::actor(zsock_t* pipe, void* vargs)
{
    dn::Node node(pipe, vargs);
    zsock_signal(pipe, 0);      // ready
    node.run();
}
