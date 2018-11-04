#ifndef dexnet_portset_h_seen
#define dexnet_portset_h_seen

#include "dexnet/port.h"

#include <czmq.h>

#include <string>
#include <vector>
#include <memory>

#include <unordered_map>


namespace dexnet {
    namespace node {

        // A collection of Port and various ways to look them up.
        // Note, returned Port pointers are owned by PortSet even
        // through they are non-const.
        class PortSet {

        public:
            PortSet();
            ~PortSet();           // destroy any owned sockets

            // Make and own a socket of the given type.  Return
            // Port or nullptr;
            Port* make(const std::string& name, int zmqsocktype);

            // Add a port but don't take ownership of socket.  Same
            // return as make().
            Port* add(const std::string& name, zsock_t* sock);

            // Lookup port data by name, return nullptr if not found
            Port* find(const std::string& name);

            // lookup port ID by socket, return nullptr if not found
            Port* find(zsock_t* sock);

            // Return pointer to Port with ID or nullptr.
            Port* get(portid_t pid);

            // Return vector of all valid port IDs
            typedef std::vector<portid_t> pids_t;
            pids_t pids();
            
            // return bound and connected endpoints
            // ....tbd...

        private:


            typedef std::vector< std::unique_ptr<Port> > store_t;
            typedef std::unordered_map<zsock_t*,size_t> sockindex_t;
            typedef std::unordered_map<std::string,size_t> nameindex_t;

            store_t     m_store;
            sockindex_t m_indbysock;
            nameindex_t m_indbyname;

        };
    }
}


#endif
