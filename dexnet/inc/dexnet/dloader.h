#ifndef DEXNET_DLOADER_H_SEEN
#define DEXNET_DLOADER_H_SEEN

#include <czmq.h>

namespace dexnet {
    namespace dloader {

        struct config {
            const char* endpoint;
        };

        void actor(zsock_t* pipe, void* vargs);

    }
}

#endif
