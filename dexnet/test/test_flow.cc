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
        std::cerr << "usage: test_flow config.json\n";
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
        zactor_destroy(&actor);
    }

    return 0;
}
