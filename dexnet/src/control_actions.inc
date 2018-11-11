// -*- c++ -*- //
#include "control_actions.h"
#include "dexnet/protocol.h"

#include "czmqpb.h"

namespace dh = dexnet::czmqpb;
namespace dn = dexnet::node;
namespace dnc = dexnet::node::control;

void dnc::actConnect::operator()(const evConnect& evin,
                                 boost::sml::back::process<evOK> process_event,
                                 boost::sml::back::process<evFail> process_fail) 
{
    zsys_debug("connect action");
    zmsg_t* msg = evin.port->msg();
    
    dnc::Connect conn;
    int rc = dh::get_frame(msg, 1, conn);
    if (rc < 0) {
        zsys_error("control: failed to get frame for connect action");
        process_fail(evFail{});
    }
    std::string name = conn.portname();
    std::string ep = conn.endpoint();
    dn::PortSet& ps = evin.node->ports();
    dn::Port* other = ps.find(name);
    if (!other) {
        zsys_error("control: failed to get port %s for connect action", name.c_str());
        process_fail(evFail{});
    }
    rc = other->connect(ep);
    if (rc < 0) {
        zsys_error("control: failed to connect \"%s\" to endpoint \"%s\"",
                   name.c_str(), ep.c_str());
        process_fail(evFail{});
    }
    process_event(evOK{});
}


/*
  event: evStatus
  event: boost::sml::v1_1_0::back::sm_impl<boost::sml::v1_1_0::back::sm_policy<fsm_maker> >
  deps: boost::sml::v1_1_0::aux::pool<>
  subs: boost::sml::v1_1_0::aux::pool<boost::sml::v1_1_0::back::sm_impl<boost::sml::v1_1_0::back::sm_policy<fsm_maker> > >
*/
//auto actStatus = [](const auto& evin, auto& sm, auto& deps, auto& subs) {
void dnc::actStatus::operator()(const evStatus& evin, 
                                boost::sml::back::process<evOK> process_ok,
                                boost::sml::back::process<evFail> process_fail) {
//auto& evin, auto& sm, auto& deps, auto& subs) {

//    zsys_debug("status action:\n\tevent: %s\n\tevent: %s\n\tdeps: %s\n\tsubs: %s",
//               tname(evin), tname(sm), tname(deps), tname(subs)
//        );

    dnc::State st;
    st.set_nodename(evin.node->name());
    st.set_payload(evin.node->payload()->name());
    dn::PortSet& ps = evin.node->ports();
    for (int pid : ps.pids()) {
        dn::Port* p = ps.get(pid);
        auto pp = st.add_states();
        pp->set_portid(pid);
        pp->set_portname(p->name());
        // fixme: there's more to transfer but leave it for now.
    }

    const int pcid = 0;     // fixme
    dnc::Header header;
    header.set_pcid(pcid);
    header.set_msgid(dh::msg_id(st));

    auto* pactor = ps.find("actor");
    zmsg_t* msg = pactor->create();
    dh::append_frame(msg, header);
    dh::append_frame(msg, st);
    int rc = pactor->send();

    if (rc != 0) {
        zsys_error("failed to send status");
        //sm.process_event(evFail{}, deps, subs);
        process_fail(evFail{});
        return;
    }
    //sm.process_event(evOK{}, deps, subs);
    process_ok(evOK{});
}


