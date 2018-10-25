#include "dexnet/dloader.h"
#include "dexnet/protohelpers.h"
#include "pb/dloader.pb.h"


namespace dd = dexnet::dloader;
namespace dh = dexnet::helpers;

int main()
{
    int rc=0;
    typedef short int sample_type;
    const size_t chunk = 960;   // samples per chunk
    const size_t stride = 960;   // strides between start of neighboring chunks
    const size_t nchunks = 2000; // chunks per send
    const size_t nsends = 1000;  // number of sends before quitting
    const size_t sample_size = sizeof(sample_type);
    const size_t nsamples = chunk*nchunks*nsends;
    const size_t nbytes = nsamples*sample_size;

    zsys_init();
    zsys_set_logident("test_dloader");


    const char* datafile = "test_dloader.dat";
    {
        struct stat st;
        rc = stat(datafile, &st);
        if (rc != 0) {
            zsys_info("tdloader: no data file found: %s", datafile);
            zsys_info("tdloader: Eg run: /bin/dd if=/dev/urandom of=%s bs=1M count=%d",
                      datafile, nbytes/(1024*1024));
            // 47 seconds for 7324M
            // to immediately read to /dev/null 9.7 GB/s
            // after three syncs, 8.5GB/s.  WTF!!
            // 05:00.0 Non-Volatile memory controller: Samsung Electronics Co Ltd NVMe SSD Controller SM961/PM961
            // nice storage unit....
            // 9m, 14 MB/s to write on hierocles /dev/sda which is some older SSD
            // 12.3s 627 MB/s to read to /dev/null
            return -1;
        }
    }
    const char* stream_address = "inproc://loader";
    zsock_t* stream = zsock_new(ZMQ_PAIR);
    assert(stream);
    rc = zsock_connect(stream, stream_address, NULL); // note, connect before bind! :)
    assert (rc == 0);

    dd::config cfg{stream_address};
    zactor_t* loader = zactor_new(dd::actor, (void*)&cfg);
    assert (loader);
    zsock_t* pipe = zactor_sock(loader);
    assert (pipe);
    {
        rc = dh::send_msg(dd::AskPort{}, pipe);
        zsys_debug("tdloader: send returns %d", rc);
        assert(rc == 0);
        
        zmsg_t* msg = zmsg_recv(pipe);
        zframe_t* fid = zmsg_pop(msg);
        int id = dh::msg_id(fid);
        zsys_info("tdloader: message id: %d", id);
        zframe_destroy(&fid);
        assert(id == dh::msg_id<dd::Port>());

        zframe_t* frame = zmsg_pop(msg);
        dd::Port portmsg;
        dh::read_frame(frame, portmsg);
        zsys_info("tdloader: PORT = %d", portmsg.port());
    
        zframe_destroy(&frame);
        zmsg_destroy(&msg);
    }

    // this should do nothing as we've not yet loaded
    dd::Start start;
    start.set_delay(1);
    start.set_nchunks(nchunks);
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    dd::Load load;
    load.set_filename(datafile);
    load.set_offset(0);
    load.set_stride(stride);
    load.set_chunk(chunk);
    rc = dh::send_msg(load, pipe);
    assert(rc == 0);

    // resend, should now take because we've loaded
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    zsys_debug("tdloader: starting to read stream");
    auto tbeg = zclock_usecs();
    zpoller_t* poller = zpoller_new(pipe, stream, NULL);
    int nrecv = 0;
    while (true) {
        void *which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("test_dloader interupted");
            break;
        }

        if (which == stream) {
            zmsg_t* msg = zmsg_recv(stream);
            assert(msg);
            zmsg_destroy(&msg);
            ++nrecv;
            continue;
        }

        if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            zframe_t* fid = zmsg_pop(msg);
            if (dh::msg_id(fid) == dh::msg_id<dd::Exhausted>()) {
                dd::Exhausted done;
                zframe_t* frame = zmsg_pop(msg);
                dh::read_frame(frame, done);
                auto tend = zclock_usecs();
                zsys_info("tdloader: Ending test: %d in %f s", nrecv, 1e-6*(tend-tbeg));
                zframe_destroy(&frame);
            }
            else {
                zsys_error("tdloader: unexpected message: %d", dh::msg_id(fid));
            }
            zframe_destroy(&fid);
            zmsg_destroy(&msg);
            break;
        }

    }

    zpoller_destroy(&poller);
    zsock_destroy(&stream);
    zactor_destroy(&loader);
    return 0;
}
