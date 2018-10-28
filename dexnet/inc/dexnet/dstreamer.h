#ifndef DEXNET_DSTREAMER_H_SEEN
#define DEXNET_DSTREAMER_H_SEEN

#include <czmq.h>

#include <vector>
#include <string>

namespace dexnet {
    namespace dstreamer {

        typedef uint16_t sample_t;

        struct config {
            std::vector<std::string> endpoints;
        };

        void actor(zsock_t* pipe, void* vargs);

    }
}

#endif
