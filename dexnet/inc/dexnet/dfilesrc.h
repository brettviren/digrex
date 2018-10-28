#ifndef DEXNET_DFILESRC_H_SEEN
#define DEXNET_DFILESRC_H_SEEN

#include <czmq.h>

namespace dexnet {
    namespace dfilesrc {

        typedef short int sample_t; // fixme: should be in a dribbon.h

        struct config {
            const char* endpoint;
        };

        void actor(zsock_t* pipe, void* vargs);

    }
}

#endif
