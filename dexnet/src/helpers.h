#ifndef DEXNET_PRIVATE_HELPERS
#define DEXNET_PRIVATE_HELPERS

#include <czmq.h>
#include <string>

namespace helpers {
    std::string popstr(zmsg_t* msg);

    struct sock_link_t {
        zsock_t* sock;
        int port;
        sock_link_t(zsock_t* s, int p=-1) : sock(s), port(p) {}

        bool ready() { return sock and port >= 0; }
    };
    
}

#endif
