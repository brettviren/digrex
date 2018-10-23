#include "dexnet/dloader.h"
#include "dexnet/protohelpers.h"
#include "pb/dloader.pb.h"


namespace dd = dexnet::dloader;
namespace dh = dexnet::helpers;

int main()
{
    int rc=0;
    typedef short int sample_type;
    const size_t chunk = 960;
    const size_t nchunks = 2000;
    const size_t nsends = 1000;
    const size_t sample_size = sizeof(sample_type);
    const size_t nsamples = chunk*nchunks*nsends*sample_size;
    const size_t nbytes = nsamples*sample_size;

    zsys_init();
    zsys_set_logident("test_dloader");


    const char* datafile = "test_dloader.dat";
    {
        struct stat st;
        rc = stat(datafile, &st);
        if (rc != 0) {
            zsys_info("no data file found: %s", datafile);
            zsys_info("Eg run: /bin/dd if=/dev/urandom of=%s bs=1M count=%d",
                      datafile, nbytes/(1024*1024));
            // 47 seconds for 7324M
            // to immediately read to /dev/null 9.7 GB/s
            // after three syncs, 8.5GB/s.  WTF!!
            // 05:00.0 Non-Volatile memory controller: Samsung Electronics Co Ltd NVMe SSD Controller SM961/PM961
            // nice storage unit....
            return -1;
        }
    }
    dd::config cfg{"inproc://loader"};
    zactor_t* loader = zactor_new(dd::actor, (void*)&cfg);
    assert (loader);
    zsock_t* pipe = zactor_sock(loader);
    assert (pipe);
    {
        rc = dh::send_msg(dd::AskPort{}, pipe);
        zsys_debug("send returns %d", rc);
        assert(rc == 0);
        
        zmsg_t* msg = zmsg_recv(pipe);
        zframe_t* fid = zmsg_pop(msg);
        int id = dh::msg_id(fid);
        zsys_info("message id: %d", id);
        zframe_destroy(&fid);
        assert(id == dh::msg_id<dd::Port>());

        zframe_t* frame = zmsg_pop(msg);
        dd::Port portmsg;
        dh::read_frame(frame, portmsg);
        zsys_info("PORT = %d", portmsg.port());
    
        zframe_destroy(&frame);
        zmsg_destroy(&msg);
    }

    // this should do nothing as we've not yet loaded
    dd::Start start;
    start.set_delay(1);
    start.set_nchunks(1000);
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    dd::Load load;
    load.set_filename(datafile);
    load.set_offset(0);
    load.set_stride(480);
    load.set_chunk(480);
    rc = dh::send_msg(load, pipe);
    assert(rc == 0);

    // resend, should now take because we've loaded
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    zsys_info("Ending test");

    zactor_destroy(&loader);
    return 0;
}
