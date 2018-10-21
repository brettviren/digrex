#include "dexnet/dloader.h"
#include "dexnet/protohelpers.h"
#include "pb/dloader.pb.h"

namespace dd = dexnet::dloader;
namespace dh = dexnet::helpers;

int main()
{
    zsys_init();
    zsys_set_logident("test_dloader");

    dd::config cfg{"inproc://loader"};
    zactor_t* loader = zactor_new(dd::actor, (void*)&cfg);
    zsock_t* pipe = zactor_sock(loader);

    int rc=0;

    rc = dh::send_msg(dd::AskPort{}, pipe);
    assert(rc == 0);

    // this should do nothing as we've not yet loaded
    dd::Start start;
    start.set_delay(1);
    start.set_nchunks(1000);
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    dd::Load load;
    load.set_filename("test_dloader.dat");
    load.set_offset(0);
    load.set_stride(480);
    load.set_chunk(480);
    rc = dh::send_msg(load, pipe);
    assert(rc == 0);

    // resend, should now take because we've loaded
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    zmsg_t* msg = zmsg_recv(pipe);
    zframe_t* fid = zmsg_pop(msg);
    auto id = dh::msg_id(fid);
    zframe_destroy(&fid);
    zsys_info("message %d", id);
        
    zframe_t* frame = zmsg_pop(msg);
    zmsg_destroy(&msg);

    assert(id == dh::msg_id<dd::Port>());
    dd::Port portmsg;
    dh::read_frame(frame, portmsg);
    zsys_info("PORT = %d", portmsg.port());

    zframe_destroy(&frame);


    zactor_destroy(&loader);
    return 0;
}
