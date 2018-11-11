// This contains helper functions which bind CZMQ and Protocol
// Buffers.  It's kept private to libdexnet so avoid exporing this
// tight binding to any plugins which may be developed in the future.

#ifndef dexnet_private_czmqpb_h_seen
#define dexnet_private_czmqpb_h_seen

#include <czmq.h>
#include <google/protobuf/message.h>

namespace dexnet {

    namespace czmqpb {

        // Messages IDs are taken to be the 1-based count of the
        // message type in the protocol.  It's 1+ the protobuf index.
        // Note, a protocol message is held in a ZMQ message *frame*.
        // A ZMQ message may thus contain have zero or more protocol
        // messages.  Fixme: this confusion represents are multiple
        // overlapping taxonomies with degenerate terms.  should clear
        // this up!
        template<typename ProtobufType>
        int msg_id() {
            return 1 + ProtobufType::descriptor()->index() ;
        }

        // Return the message id of the PB object.
        int msg_id(const ::google::protobuf::Message& pb);

        // Return the ID assuming the frame is an ID frame. ID=-1 means
        // $TERM. Ownership is not taken.
        int msg_id(zframe_t* frame);

        // The message ID is also stored as the first frame of the zmsg_t.
        // This sets the "cursor" to the first frame so the message must
        // be mutable but no other changes are done to the msg.
        int msg_id(zmsg_t* msg);

        // return the message type name given the PB type
        template<typename ProtobufType>
        const std::string& msg_name() {
            return ProtobufType::descriptor()->name();
        }

        // Return the message type name of the PB object.
        const std::string& msg_name(const ::google::protobuf::Message& pb);

        // return true if msg is special CZMQ $TERM.
        bool msg_term(zmsg_t* msg);

        // Fill pb object from frame at index in message.  Return rc.
        int get_frame(zmsg_t* msg, int frame_index, ::google::protobuf::Message& pb);

        // Append frame to end of message filled with serialized pb object.  Return rc.
        int append_frame(zmsg_t* msg, ::google::protobuf::Message& pb);

    }
}



#endif
