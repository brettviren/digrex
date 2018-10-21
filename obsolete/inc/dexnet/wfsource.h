#ifndef DEXNET_WFSOURCE
#define DEXNET_WFSOURCE

#include <czmq.h>

namespace dexnet {

    namespace wfsource {

        void actor(zsock_t* pipe, void* vargs);
    }
}

#endif
