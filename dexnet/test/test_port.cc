#include "dexnet/port.h"

namespace dn = dexnet::node;

int main()
{
    {
        dn::PortSet ports;
        auto pd1 = ports.add("",nullptr);
        assert(pd1);
        auto pd2 = ports.make("apair",ZMQ_PAIR);
        assert(pd2 == 0);
        auto pids = ports.pids();
        assert(pids.size() == 2);
    }

    return 0;
}
