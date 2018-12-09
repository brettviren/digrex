/*
  This provides a generic command line interface to a dexnet agent and
  through it some number of nodes as driven by dexnet configuration.
 */

#include "json.hpp"
#include "upif.h"

#include <string>
#include <fstream>
#include <iostream>

// need to define configuration schema
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "usage: dexnet-agent cfg.json\n"
                  << "hint:  dexnet-agent <(jsonnet cfg.jsonnet)\n";
        return -1;
    }
    json jcfg;
    std::ifstream fstr(argv[1]);
    fstr >> jcfg;

    // "{plugins: []}"
    upif::cache pic;
    auto plugin = pic.add("dexnet");
    for (auto jp : jcfg["plugins"]) {
        std::string piname = jp;
        auto plugin = pic.add(piname);
    }

    // "{agent: {name:<name>, ...}}"
    json agent_cfg = jcfg["agent"];
    std::string agent_name = agent_cfg["name"];
    auto agent_maker = pic.find(agent_name);

    // // run agent

    return 0;
}
