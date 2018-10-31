#ifndef dexnet_node_h_seen
#define dexnet_node_h_seen

#include <czmq.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace dexnet {

    namespace node {

        struct Port {
            std::string name{};
            zsock_t* socket{};
            std::unordered_map<std::string, int> bound{}; // endpoint string and port number
            std::unordered_set<std::string> connected{};

            int bind(const std::string& endpoint);
            int connect(const std::string& endpoint);
        };

        typedef std::vector<Port> portlist_t;
        typedef std::unordered_map<zsock_t*,size_t> sockindex_t;
        typedef std::unordered_map<std::string,size_t> stringindex_t;

        // Address ports by socket, name or index
        class PortSet {
            portlist_t    m_ports;
            sockindex_t   m_indbysock;
            stringindex_t m_indbyname;

        public:
            PortSet();
            size_t add(const Port& p);
            Port& get(const std::string& name);
            Port& get(zsock_t* sock);
            Port& get(size_t ind) { return m_ports[ind]; }
            bool have(const std::string& name);
            bool have(zsock_t* sock);
            portlist_t& asvector() { return m_ports; }
        };


        // A general node context
        class Node {
            PortSet m_ports;
            std::string m_name;
        public:
            Node(zsock_t* pipe, void* args);
            ~Node();
            void run();

            const std::string& name() { return m_name; }

            int input(zsock_t* sock);

        private:
            void initialize(const std::string& json_text);
        };

        void actor(zsock_t* pipe, void* vargs);
    }
}
#endif
