#ifndef dexnet_node_h_seen
#define dexnet_node_h_seen

#include "dexnet/portset.h"
#include "upif.h"
#include "json.hpp"
#include <string>

namespace dexnet {

    namespace node {

        // A general node context
        class Node {
        public:
            Node(zsock_t* pipe, void* args);
            ~Node();
            void run();

            int input(zsock_t* sock);
            int timer(int timer_id);

            std::string name() { return m_name; }
            PortSet& ports() { return m_ports; }
            Protocol* payload() { return m_payload; }
            

        private:
            void initialize(const std::string& json_text);

            // internal: initialize a protocol.  If no port name then
            // it's assumed to be the "payload" protocol.
            void initialize_protocol(nlohmann::json& jcfg, const std::string& portname="");
            void shutdown();

            std::string m_name;
            PortSet m_ports;
            Protocol* m_payload;
            zloop_t* m_loop;
            upif::cache m_plugins;
            std::unordered_map<int, Protocol*> m_timers;
        };

        void actor(zsock_t* pipe, void* vargs);
    }
}
#endif
