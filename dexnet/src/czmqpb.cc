#include "czmqpb.h"

namespace dh = dexnet::czmqpb;

int dh::msg_id(const ::google::protobuf::Message& pb) {
    return 1 + pb.GetDescriptor()->index();
}

const std::string& dh::msg_name(const ::google::protobuf::Message& pb) {
    return pb.GetDescriptor()->name();
}

int dh::msg_id(zframe_t* frame) {
    if (zframe_streq(frame, "$TERM")) {
        return 0;
    }
    int id = *(int*)zframe_data(frame);
    assert(id > 0);
    return id;
}

int dh::msg_id(zmsg_t* msg) {
    zframe_t* one = zmsg_first(msg);
    return dh::msg_id(one);
}
bool dh::msg_term(zmsg_t* msg) {
    zframe_t* one = zmsg_first(msg);
    return zframe_streq(one, "$TERM");
}

int dh::get_frame(zmsg_t* msg, int frame_index, ::google::protobuf::Message& pb)
{
    if (!msg) {
        zsys_error("dnp: no message");
        return -1;
    }
    zframe_t* frame = zmsg_first(msg);
    if (!frame) {
        zsys_error("dnp: failed to get first frame");
        return -1;
    }
    while (frame_index) {
        frame = zmsg_next(msg);
        if (!frame) {
            zsys_error("dnp: failed to get frame %d", frame_index);
            return -1;
        }
        --frame_index;
    }
    bool ok = pb.ParseFromArray(zframe_data(frame), zframe_size(frame));
    if (ok) return 0;
    zsys_error("dnp: PB failed to parse");
    return -1;
}

int dh::append_frame(zmsg_t* msg, ::google::protobuf::Message& pb)
{
    if (!msg) {
        zsys_error("null msg");
        return -1;
    }
    zframe_t* frame = zframe_new(NULL, pb.ByteSize());
    bool ok = pb.SerializeToArray(zframe_data(frame), zframe_size(frame));
    if (!ok) {
        zsys_error("failed to serialize");
        return -1;
    }
    zmsg_append(msg, &frame);
    return 0;
}



