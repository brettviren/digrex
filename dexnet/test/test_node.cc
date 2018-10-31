#include "dexnet/node.h"

namespace dn = dexnet::node;

int main()
{
    zactor_t* actor = zactor_new(dn::actor, NULL);
    zactor_destroy(&actor);

    return 0;
}
