#ifndef DEXNET_DPUSHER_H_SEEN
#define DEXNET_DPUSHER_H_SEEN

#include <czmq.h>

#include <vector>
#include <string>

namespace dexnet {
    namespace dpusher {

        typedef uint16_t sample_t;

        struct config {
            const char* endpoint; // PUSH endpoint
        };

        void actor(zsock_t* pipe, void* vargs);

    }
}

#endif
