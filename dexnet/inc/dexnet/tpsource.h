#ifndef DEXNET_TPSOURCE
#define DEXNET_TPSOURCE

#include <czmq.h>

namespace dexnet {

    namespace tpsource {

        void actor(zsock_t* pipe, void* vargs);
    }
}

#endif
