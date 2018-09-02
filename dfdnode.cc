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


class PASource {
    zmq::socket_t& m_out;
    int m_fragid;
public:
    PASource(std::vector<zmq::socket_t*>& inputs,
             std::vector<zmq::socket_t*>& outputs,
             json& jcfg)
        : m_out(*outputs[0])
        , m_fragid(jcfg["fragment_ident"]) { }

    void operator()() {
        const time_t t0 = time(0);

        int count = 0;
        while (true) {
            count += 1;
            const time_t t = time(0);
            const int ref_tick = int((t-t0)/ 0.5e-6);
        
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
            m_out.send (reply);
        }
    }
};

std::vector<zmq::socket_t*> make_sockets(zmq::context_t& context, json& jcfg)
{
    std::vector<zmq::socket_t*> ret;
    for (auto jone : jcfg) {
        int socktype = jone["socktype"];
        std::string url = jone["url"];
        std::string meth = jone["method"];

        zmq::socket_t* sock = new zmq::socket_t(context, socktype);
        if (meth == "bind") {
            sock->bind(url);
        }
        else if (meth == "connect") {
            sock->connect(url);
        }
        else {
            throw std::runtime_error("unknown socket method: "+meth);
        }

        ret.push_back(sock);
    }
    return ret;
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

    json jcfg;
    {
        std::ifstream cfgstream(argv[1]);
        cfgstream >> jcfg;
    }

    auto jrole = jcfg["role"];
    const std::string role_type = jrole["type"];

    // This has to be thread local.  For now, we just do one
    // context/node per executable.
    zmq::context_t context (1);    

    std::vector<zmq::socket_t*> inputs, outputs;

    if (role_type == "PASource") {
        inputs = make_sockets(context, jrole["inputs"]);
        outputs = make_sockets(context, jrole["outputs"]);

        PASource pas(inputs, outputs, jrole);
        pas();
    }
    // else{}

    for (auto sockptr : inputs) {
        delete sockptr;
    }
    for (auto sockptr : outputs) {
        delete sockptr;
    }

    return 0;
}
