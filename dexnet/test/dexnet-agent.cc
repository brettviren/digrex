/*
  This provides a generic command line interface to a dexnet agent and
  through it some number of nodes as driven by dexnet configuration.
 */
#include "dexnet/node.h"
#include "json.hpp"
#include "upif.h"
#include <czmq.h>

#include <string>
#include <fstream>
#include <iostream>

// need to define configuration schema
using json = nlohmann::json;

namespace dn = dexnet::node;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "usage: dexnet-agent cfg.json\n"
                  << "hint:  dexnet-agent <(jsonnet cfg.jsonnet)\n";
        return -1;
    }
    zsys_init();
    zsys_set_logident("dexnet-agent");

    json jcfg;
    std::ifstream fstr(argv[1]);
    fstr >> jcfg;

    // "{plugins: []}"
    auto plugin = upif::add("dexnet");
    for (auto jp : jcfg["plugins"]) {
        std::string piname = jp;
        auto plugin = upif::add(piname);
    }

    std::unordered_map<zactor_t*, std::string> actors; // node actors
    for (auto jn : jcfg["nodes"]) {
        std::string nam = jn["name"];
        std::string typ = jn["type"];
        std::string jtext = jn.dump();
        zactor_t* actor = zactor_new(dn::actor, (void*)jtext.c_str());
        assert(actor);
        actors[actor] = typ+":"+nam;
    }

    zpoller_t* poller = zpoller_new(NULL);
    for (auto& ait : actors) {
        zsys_info("polling %s", ait.second.c_str());
        zpoller_add(poller, ait.first);
    }

    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("interupted");
            break;
        }
        auto ait = actors.find((zactor_t*)which);
        if (ait == actors.end()) {
            zsys_warning("heard from uknown actor");
            // fixme: read and drop?
        }
        zsys_info(ait->second.c_str());
        // fixme: read, maybe print and drop message?
    }

    zpoller_destroy(&poller);
    for (auto ait : actors) {
        zsys_debug("destroy actor %s", ait.second.c_str());
        zactor_t* actor = ait.first;
        zactor_destroy(&actor);
    }


    return 0;
}
