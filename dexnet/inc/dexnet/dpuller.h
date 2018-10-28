#ifndef DEXNET_DPULLER_H_SEEN
#define DEXNET_DPULLER_H_SEEN

#include <czmq.h>

#include <vector>
#include <string>

namespace dexnet {
    namespace dpuller {

        typedef uint16_t sample_t;

        struct config {
            // nada
        };

        void actor(zsock_t* pipe, void* vargs);

    }
}

#endif
