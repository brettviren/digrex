#include "dexnet/node.h"
#include "json.hpp"

#include "pb/testcontrol.pb.h"

#include <string>
#include <iostream>

using json = nlohmann::json;

namespace dn = dexnet::node;
namespace dnc = dexnet::node::control;

int main()
{
    auto jcfg = R"(
{
   "name": "testcontrol",
   "ports": [ ],
   "type": "control",
   "plugins": [ { "name": "dexnet" } ]
}
)"_json;

    std::string jstr = jcfg.dump();
    zactor_t* actor = zactor_new(dn::actor, (void*)jstr.c_str());

    {
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
    {
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

    zactor_destroy(&actor);

    return 0;
}
