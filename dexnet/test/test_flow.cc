// test flow source/split/sink

#include "dexnet/node.h"
#include "json.hpp"

#include "pb/testcontrol.pb.h"
#include "pb/testflow.pb.h"

#include <string>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace dn = dexnet::node;
namespace dnc = dexnet::node::control;
namespace dnf = dexnet::node::flow;

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "usage: test_flow <(jsonnet jsonnet/test_flow.jsonnet)\n";
        return -1;
    }

    json jcfg;
    std::ifstream fstr(argv[1]);
    fstr >> jcfg;

    std::vector<zactor_t*> actors; // node actors

    for (auto jone : jcfg) {
        std::string jtext = jone.dump();
        zactor_t* actor = zactor_new(dn::actor, (void*)jtext.c_str());
        assert(actor);
        actors.push_back(actor);
    }

    // actually test something here
    for (auto actor : actors) {
        zmsg_t* msg = zmsg_new();
        dnc::Header header;
        header.set_pcid(0);
        header.set_msgid(2);
        zframe_t* frame = zframe_new(NULL, header.ByteSize());
        header.SerializeToArray(zframe_data(frame), zframe_size(frame));
        zmsg_append(msg, &frame);
        int rc = zactor_send(actor, &msg);
        assert(rc == 0);
        zsys_debug("sent status");
    }
    for (auto actor: actors) {
        zmsg_t* msg = zmsg_recv(zactor_sock(actor));
        assert(msg);

        zframe_t* fhead = zmsg_first(msg);
        assert(fhead);
        dnc::Header header;
        header.ParseFromArray(zframe_data(fhead), zframe_size(fhead));

        std::cout << "header: " << header.pcid() << ":" << header.msgid() << std::endl;

        zframe_t* frame = zmsg_next(msg);
        assert(frame);
        dnc::State st;
        st.ParseFromArray(zframe_data(frame), zframe_size(frame));
        std::cout << st.nodename() << std::endl;
    }


    for (auto actor : actors) {
        zactor_destroy(&actor);
    }

    return 0;
}
