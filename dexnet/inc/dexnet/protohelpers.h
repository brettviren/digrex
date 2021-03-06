#ifndef dexnet_protohelpers_h_seen
#define dexnet_protohelpers_h_seen
#include <string>
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

        inline
        int msg_id(const ::google::protobuf::Message& pb) {
            return 1 + pb.GetDescriptor()->index();
        }

        template<typename ProtobufType>
        const std::string& msg_name() {
            return ProtobufType::descriptor()->name();
        }
        inline
        const std::string& msg_name(const ::google::protobuf::Message& pb) {
            return pb.GetDescriptor()->name();
        }

        // Return the ID assuming the frame is an ID frame. ID=-1 means
        // $TERM. Ownership is not taken.
        inline
        int msg_id(zframe_t* frame) {
            if (zframe_streq(frame, "$TERM")) {
                return 0;
            }
            int id = *(int*)zframe_data(frame);
            assert(id > 0);
            return id;
        }

        // The message ID is also stored as the first frame of the zmsg_t.
        // This sets the "cursor" to the first frame so the message must
        // be mutable but no other changes are done to the msg.
        inline
        int msg_id(zmsg_t* msg) {
            zframe_t* one = zmsg_first(msg);
            return msg_id(one);
        }

        // When CZMQ sends "$TERM" as first frame it's first 4 chars
        // get cast to this number.  This function is probably not
        // portable....
        inline
        bool msg_term(int id) {
            return id == 0x52455424;
        }
        inline
        bool msg_term(zmsg_t* msg) {
            zframe_t* one = zmsg_first(msg);
            return zframe_streq(one, "$TERM");
        }

        // Make a frame filled with the protobuf.  Caller takes ownership.
        inline
        zframe_t* make_frame(const ::google::protobuf::Message& pb) {
            size_t siz = pb.ByteSize();
            //zsys_debug("make frame id %d (%s) [%d]",
            //           msg_id(pb), msg_name(pb).c_str(), pb.ByteSize());
            if (siz > 0) {
                zframe_t* frame = zframe_new(NULL, pb.ByteSize());
                pb.SerializeToArray(zframe_data(frame), zframe_size(frame));
                return frame;
            }
            return zframe_new(NULL, 0);
        }
        inline
        zframe_t* make_id_frame(int id) {
            zframe_t* fid = zframe_new(&id, sizeof(int));
            assert(fid);
            return fid;
        }

        // Make a message with first frame holding ID and second holding
        // protobuf.  Caller takes ownership.
        inline
        zmsg_t* make_msg(const ::google::protobuf::Message& pb) {
            zmsg_t* msg = zmsg_new();
            int id = msg_id(pb);
            //zsys_debug("make message id %d (%s)", id, msg_name(pb).c_str());
            zframe_t* fid = make_id_frame(id);
            zmsg_append(msg, &fid);
            zframe_t* frame = make_frame(pb);
            assert (frame);
            int rc = zmsg_append(msg, &frame);
            assert(rc == 0);
            return msg;
        }

        inline
        int send_msg(const ::google::protobuf::Message& pb, zsock_t* sock) {
            zmsg_t* msg = make_msg(pb);
            assert(msg);
            //zsys_debug("sending msg id %d (%s)", msg_id(pb), msg_name(pb).c_str());
            return zmsg_send(&msg, sock);
        }

        inline
        void read_frame(zframe_t* frame, ::google::protobuf::Message& obj) {
            obj.ParseFromArray(zframe_data(frame), zframe_size(frame));
        }
    
    }
} // namespace

#endif
