#include <zmq.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <ctime>
#include <exception>

#include "json.hpp"
#include "dune.pb.h"

using json = nlohmann::json;

const char PA_TOPIC = 'P';

const int chirp_count = 100000;

typedef std::map<std::string, zmq::socket_t*> port_map_t;

class PASource {
    zmq::socket_t* m_outbox;
    int m_fragid;
public:
    PASource(json& jcfg, port_map_t& ports) 
        : m_outbox(ports["outbox"])
        , m_fragid(jcfg["params"]["fragid"]) { }

    bool operator()() {
        const time_t t0 = time(0);

        int count = 0;
        while (true) {
            count += 1;
            const time_t t = time(0);
            const int dt = t-t0;
            const int ref_tick = int(dt/ 0.5e-6);
        
            PrimitiveActivity pa;
            pa.set_fragid(m_fragid);
            pa.set_count(count);
            pa.set_reference_tick(ref_tick);

            for (int ind = 0; ind < 10; ++ind) {
                auto * ca = pa.add_primitives();
                ca->set_channel(ind+48);
                ca->set_start_tick(40);
                ca->set_duration(std::abs(ind-5));
                ca->set_activity((ind-5)*(ind-5));
            }

            std::string dat;
            pa.SerializeToString(&dat);

            zmq::message_t reply (dat.size());
            memcpy ((void *) reply.data (), dat.c_str(), dat.size());
            m_outbox->send(reply);
            if (dt > 0 && count % chirp_count == 0) {
                double khz = count / dt * 0.001;
                std::cerr << count / chirp_count << " "
                          << dt << " "
                          << ref_tick << " "
                          << khz << " kHz\n";
            }
        }
        return true;
    }
};

class PASink {
    zmq::socket_t* m_inbox;
public:
    PASink(json& jcfg, port_map_t& ports) 
        : m_inbox(ports["inbox"]) { }

    bool operator()() {
        
        while (true) {
            zmq::message_t msg;
            m_inbox->recv(&msg);
            PrimitiveActivity pa;
            pa.ParseFromArray(msg.data(), msg.size());
            std::cerr << pa.count() << std::endl;
        }

        return true;
    }
};
    
    

zmq::socket_t* make_port(zmq::context_t& context, json& jport)
{
    int socktype = jport["type"];
    int type = jport["type"];
    const std::string meth = jport["meth"];
    const std::string url = jport["url"];
    zmq::socket_t* sock = new zmq::socket_t(context, type);

    // deal with any opts
    for (auto& jopt : jport["sockopts"]) {
        int opt = jopt["opt"];
        std::string arg = jopt["arg"];
        sock->setsockopt(opt, arg.c_str(), arg.size());
        std::cerr << jport["parent"] << ":" << jport["name"]
                  << ": setsockopt("<<opt<<", \""<<arg<<"\")\n";
    }

    if (meth == "bind") {
        sock->bind(url);
    }
    else if (meth == "connect") {
        sock->connect(url);
    }
    else {
        throw std::runtime_error("unknown socket method: "+meth);
    }
    std::cerr << jport["parent"] << ":" << jport["name"]
              << ": "<<meth<<"(\""<<type<<",\""<<url<<"\")\n";

    return sock;    
}


int main (int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr
            << "usage: dfdnode cfgfile"
            << std::endl;
        return 1;
    }

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    json jnode;
    {
        std::ifstream cfgstream(argv[1]);
        cfgstream >> jnode;
    }
    std::cerr << jnode << std::endl;


    // This has to be thread local.  For now, we just do one
    // context/node per executable.
    zmq::context_t context (1);    

    port_map_t ports;
    for (auto jone : jnode["ports"].items()) {
        std::string name = jone.key();
        auto& jport = jone.value();
        ports[name] = make_port(context, jport);
    }
             
    std::string klass = jnode["type"];
    bool ok = false;
    // Q&D factory
    if (klass == "PASource") {
        PASource node(jnode, ports);
        ok = node();
    }
    if (klass == "PASink") {
        PASink node(jnode, ports);
        ok = node();
    }

    for (auto port : ports) {
        std::cerr << jnode["type"] << ":" << jnode["name"]
                  << " deleting port " << port.first << std::endl;
        delete port.second;
    }

    return ok ? 0 : -1;
}
