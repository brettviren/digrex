/*
 */

#ifndef dexnet_port_h_seen
#define dexnet_port_h_seen

#include <czmq.h>

#include <string>
#include <vector>
#include <memory>

#include <unordered_set>
#include <unordered_map>

namespace dexnet {

    namespace node {

        // see node.h
        class Node;

        // see protocol.h;
        class Protocol;

        // Identify a port by number in the context of a PortSet.
        typedef int portid_t;
            
        // A Port is an aggregation of a socket and related things
        // defined in the context of a PortSet which owns it.  It may
        // be passed around but only the PortSet may destroy the
        // socket and then only if the socket is owned.
        struct Port {

            Port(portid_t id, std::string name, zsock_t* sock, bool owned);
            ~Port();

            // Bind a port's socket and remember endpoint.  ZMQ RC value returned.
            int bind(const std::string& endpoint);

            // Connect a port's socket and remember endpoint.  ZMQ RC value returned.
            int connect(const std::string& endpoint);

            void add(Protocol*  proto);

            // Handle input on self by visiting protcols.
            // Return -1 on error, 0 on okay and that input was
            // handled.  return 1 if okay but not handled.
            int handle(Node* node);

            // Accessors
            portid_t id() { return m_id; }
            const std::string& name() { return m_name; }
            zsock_t* sock() { return m_sock; }


            // Message handling.
                        
            // Receive a new message on the port and return it or
            // nullptr.  The message is always owned by the port.
            zmsg_t* recv();

            // Send the prepared message return rc.
            int send();

            // Get current message.  Message remains owned by the port.
            zmsg_t* msg();

            // initiate the creation of a message.  Additional frames
            // may be added by caller.  Port retains ownership.
            zmsg_t* create(int pcid, int msgid);

        private:

            portid_t m_id;
            std::string m_name;
            zsock_t* m_sock;
            zmsg_t* m_msg;
            bool m_socket_owned; // don't delete socket if true


            // endpoint string and port number from bind()
            std::unordered_map<std::string, int> m_bound;

            // endpoint string and port number from connect()
            std::unordered_set<std::string> m_connected;

            // pipeline of protocols that can handle activity on this
            // port.  Each is bound to this port data in 1-to-1.
            std::vector<Protocol*> m_protos; 

        };


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
