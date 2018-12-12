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

    // fixme: when considering initial configuraiton and later
    // reconfiguration we have set up two formats and info streams for
    // essentially identical purposes.  One is JSON the other is
    // protobufs.  Need to resolve this or at least limit the
    // duplication to this main() by having a JSON->PB converter.
    json jcfg;
    std::ifstream fstr(argv[1]);
    try {
        fstr >> jcfg;
    }
    catch  (json::parse_error& e) {
        std::cerr << "message: " << e.what() << '\n'
                  << "exception id: " << e.id << '\n'
                  << "byte position of error: " << e.byte << std::endl;
    }


    // "{plugins: []}"
    auto plugin = upif::add("dexnet");
    try {
        for (auto jp : jcfg["plugins"]) {
            std::string piname = jp["name"].get<std::string>();
            std::string libname = "";
            if (jp["library"].is_string()) {
                libname = jp["library"].get<std::string>();
            }
            zsys_info("add plugin %s [%s]", piname.c_str(), libname.c_str());
            auto plugin = upif::add(piname);
            if (!plugin) {
                zsys_error("failed to add plugin \"%s\"", piname.c_str());
                return -1;
            }
        }
    }
    catch  (json::type_error& e) {
        std::cerr << "message: " << e.what() << '\n'
                  << "exception id: " << e.id << std::endl;
    }

    // make actors and add them to poller
    zpoller_t* poller = zpoller_new(NULL);
    std::unordered_map<zactor_t*, std::string> actors; // node actors
    for (auto jn : jcfg["nodes"]) {
        std::string nam = jn["name"];
        std::string jtext = jn.dump();
        zactor_t* actor = zactor_new(dn::actor, (void*)jtext.c_str());
        assert(actor);
        actors[actor] = nam;
        zsys_info("polling %s", nam.c_str());
        zpoller_add(poller, actor);
    }

    // main loop
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

    // clean up
    zpoller_destroy(&poller);
    for (auto ait : actors) {
        zsys_debug("destroy actor %s", ait.second.c_str());
        zactor_t* actor = ait.first;
        zactor_destroy(&actor);
    }


    return 0;
}
