/*
 */

#ifndef dexnet_port_h_seen
#define dexnet_port_h_seen

#include <czmq.h>

#include <string>
#include <vector>
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

            // A port has a list of associated protocols
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

            // create a new message.  Additional frames
            // may be added by caller.  Port retains ownership.
            zmsg_t* create();

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


        
    }
}

#endif
