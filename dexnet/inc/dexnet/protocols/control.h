// This is an initial by-hand protocol to test out what might be automated.
// It handles the out-of-band control of a node, typiclaly over the agent pipe.

#ifndef dexnet_protocols_control_h_seend
#define dexnet_protocols_control_h_seend

#include "dexnet/protocol.h"

namespace dexnet {
    namespace node {
        namespace control {

            class Protocol : public dexnet::node::Protocol {
            public:
                virtual ~Protocol();

                // 0=handled, -1=error, 1=unhandled
                virtual int handle(Node* node, Port* pd);
            };
        }
    }
}

#endif
