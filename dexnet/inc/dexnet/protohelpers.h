#ifndef dexnet_protohelpers_h_seen
#define dexnet_protohelpers_h_seen

#include <czmq.h>
#include <google/protobuf/message.h>

namespace dexnet {

    namespace helpers {

        // Messages IDs are taken to be the 1-based count of the message
        // type in the protocol.  It's 1+ the protobuf index.
        template<typename ProtobufType>
        int msg_id() {
            return 1 + ProtobufType::descriptor()->index() ;
        }

        int msg_id(const ::google::protobuf::Message& pb) {
            return 1 + pb.GetDescriptor()->index();
        }

        // return the ID assuming the frame is an ID frame.  Ownerships is not taken.
        int msg_id(zframe_t* frame) {
            return *(int*)zframe_data(frame);
        }

        // The message ID is also stored as the first frame of the zmsg_t.
        // This sets the "cursor" to the first frame so the message must
        // be mutable but no other changes are done to the msg.
        int msg_id(zmsg_t* msg) {
            zframe_t* one = zmsg_first(msg);
            return *(int*)zframe_data(one);
        }

        // Make a frame filled with the protobuf.  Caller takes ownership.
        zframe_t* make_frame(const ::google::protobuf::Message& pb) {
            zframe_t* frame = zframe_new(NULL, pb.ByteSize());
            pb.SerializeToArray(zframe_data(frame), zframe_size(frame));
            return frame;
        }

        // Make a message with first frame holding ID and second holding
        // protobuf.  Caller takes ownership.
        zmsg_t* make_msg(const ::google::protobuf::Message& pb) {
            zmsg_t* msg = zmsg_new();
            int id = msg_id(pb);
            zmsg_addmem(msg, &id, sizeof(int));
            zframe_t* frame = make_frame(pb);
            zmsg_append(msg, &frame);
            return msg;
        }

        int send_msg(const ::google::protobuf::Message& pb, zsock_t* sock) {
            zmsg_t* msg = make_msg(pb);
            return zmsg_send(&msg, sock);
        }

        void read_frame(zframe_t* frame, ::google::protobuf::Message& obj) {
            obj.ParseFromArray(zframe_data(frame), zframe_size(frame));
        }
    
    }
} // namespace

#endif
