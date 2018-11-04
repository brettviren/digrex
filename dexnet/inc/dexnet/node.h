#ifndef dexnet_node_h_seen
#define dexnet_node_h_seen

#include "dexnet/portset.h"
#include "upif.h"

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

            std::string name() { return m_name; }
            PortSet& ports() { return m_ports; }
            Protocol* payload() { return m_payload; }
            

        private:
            void initialize(const std::string& json_text);
            void shutdown();

            // internal: add a protocol.  If no port name then it's
            // "payload" protocol.
            void addproto(const std::string& protoname,
                          const std::string& portname="");

            // fixme: might have to make this something special.
            std::string m_name;
            PortSet m_ports;
            Protocol* m_payload;
            zloop_t* m_loop;
            upif::cache m_plugins;
        };

        void actor(zsock_t* pipe, void* vargs);
    }
}
#endif
