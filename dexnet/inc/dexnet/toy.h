#ifndef dexnet_toy_h
#define dexnet_toy_h

#include <czmq.h>
#include <string>

namespace dexnet {
    namespace toy {

        namespace tpsource {
            struct sockdesc {
                int type;
                std::string addr;
                bool bind;
            };
                    

            struct config {
                sockdesc inbox, outbox;

                std::string topic;   // the topic string to publish on
            };
            zsock_t* make_sock(const sockdesc& sd);
            void actor(zsock_t* pipe, void* vargs);

        }
    }
}

#endif
