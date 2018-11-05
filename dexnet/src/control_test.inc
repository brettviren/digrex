template<typename T>
const char* tname(const T& o)
{
    int status=0;
    return abi::__cxa_demangle(typeid(o).name(), NULL, NULL, &status);
}

auto actStatus = [](const auto& evin, auto& sm, auto& deps, auto& subs) {
    zsys_debug("%s", tname(sm));

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
        sm.process_event(evFail{}, deps, subs);
        return;
    }
    sm.process_event(evOK{}, deps, subs);

};
