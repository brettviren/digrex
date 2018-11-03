#ifndef dexnet_protocol_h_seen
#define dexnet_protocol_h_seen

#include "upif.h"

namespace dexnet {
    namespace node {

        // see node.h
        class Node;

        // see port.h
        class Port;

        // The signature of a factory maker.  It should return a
        // pointer to a protocol_factory as a void* and once cast,
        // protocol_factory::create() should return a pointer to a
        // base protocol instance.
        typedef void* (*factory_maker)();

        // A concrete protocol handles activity on ports.  It must
        // also have a "make_protocol_PROTOCOLTYPE()" extern "C"
        // function that returns a protocol_factory that makes
        // protocol instances.
        struct Protocol {
            virtual ~Protocol();

            virtual std::string name() { return "base"; }

            // Return -1 on error, 0 on handled, 1 on no error but not handled.
            //
            // Subclass will likely:
            // 1) "retype" message into a typed FSM event struct.
            // 2) call process event through FSM
            // 
            virtual int handle(Node* node, Port* pd) = 0;

        };
        
        template<typename PType>
        struct ProtocolFactoryTyped {
            virtual ~ProtocolFactoryTyped() {}
            
            virtual Protocol* create(const std::string& instance_name) {
                return new PType;
            }
        };

        struct ProtocolFactory {

            virtual ~ProtocolFactory();
            
            // Create a protocol.
            //
            virtual Protocol* create(const std::string& instance_name) = 0;
        };

        // Return a protocol factory that can make a Protocol of given type name
        ProtocolFactory* protocol_factory(upif::cache& plugins,
                                          const std::string& protocol_typename);
    }
}
//extern "C" {
//    void* dexnet_protocol_factory_SOMETYPENAME(...) { return SOMETYPENAME_FACTORY(...); }
//}

#endif

