#ifndef dexnet_control_actions_h_seen
#define dexnet_control_actions_h_seen

#include "control_events.h"
#include <boost/sml.hpp>

namespace dexnet {
    namespace node {
        namespace control {

            struct actConnect {
                void operator()(const evConnect& evin, boost::sml::back::process<evOK> process_event);
            };

            struct actStatus {
                void operator()(const evStatus& evin, 
                                boost::sml::back::process<evOK> process_ok,
                                boost::sml::back::process<evFail> process_fail);
            };
        }
    }
}
#endif

