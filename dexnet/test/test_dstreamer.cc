#include "dexnet/dstreamer.h"
#include "dexnet/protohelpers.h"
#include "pb/dstreamer.pb.h"


namespace ds = dexnet::dstreamer;
namespace dh = dexnet::helpers;

int main(int argc, char* argv[])
{
    zsys_init();
    zsys_set_logident("test_dstreamer");

    if (argc < 2) {
        zsys_error("usage: test_dstreamer file.dat");
        return -1;
    }
    const char* datafile = argv[1];
    if (!zsys_file_exists(datafile)) {
        zsys_error("no such file: %s", datafile);
        return -1;
    }

    int rc=0;

    const size_t stride = 960;   // strides between start of neighboring chunks
    const size_t nchunks = 2000; // chunks per send
    const size_t nsends = 1000;  // number of sends before quitting

    const size_t nstreams = 4;

    const char* stream_address = "inproc://streamer";
    std::vector<zsock_t*> streams;
    ds::config cfg;
    for (size_t ind=0; ind != nstreams; ++ind) {
        std::string addr = stream_address;
        addr += '0'+ind;
        cfg.endpoints.push_back(addr);
        zsock_t* s = zsock_new(ZMQ_PAIR);
        assert(s);
        int rc = zsock_connect(s, addr.c_str(), NULL);
        streams.push_back(s);
    }

    zactor_t* streamer = zactor_new(ds::actor, (void*)&cfg);
    assert (streamer);
    zsock_t* pipe = zactor_sock(streamer);
    assert (pipe);

    // this should do nothing as we've not yet loaded
    ds::Start start;
    start.set_delay(1);
    start.set_nchunks(nchunks);
    start.set_nsends(nsends);
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    ds::Load load;
    load.set_filename(datafile);
    load.set_offset(0);
    load.set_stride(stride);
    rc = dh::send_msg(load, pipe);
    assert(rc == 0);

    // resend, should now take because we've loaded
    rc = dh::send_msg(start, pipe);
    assert(rc == 0);

    zsys_debug("tdstreamer: starting to read stream");
    auto tbeg = zclock_usecs();
    zpoller_t* poller = zpoller_new(pipe, NULL);
    for (auto s : streams) {
        zpoller_add(poller, s);
    }
    int nrecv = 0;
    while (true) {
        void *which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("test_dstreamer interupted");
            break;
        }

        if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            zframe_t* fid = zmsg_pop(msg);
            int id = dh::msg_id(fid);
            zframe_destroy(&fid);

            if (id == dh::msg_id<ds::Exhausted>()) {
                ds::Exhausted done;
                zframe_t* frame = zmsg_pop(msg);
                dh::read_frame(frame, done);
                auto tend = zclock_usecs();
                zsys_info("tdstreamer: data exhausted: %d in %f s", nrecv, 1e-6*(tend-tbeg));
                zframe_destroy(&frame);
                zmsg_destroy(&msg);
                break;
            }
            else {
                zsys_error("tdstreamer: unexpected message: %d", dh::msg_id(fid));
            }
            zmsg_destroy(&msg);
            continue;
        }

        // one of the streams
        zmsg_t* msg = zmsg_recv((zsock_t*)which);
        if (!msg) {
            zsys_error("tdstreamer: null message from one of the streams");
            break;
        }
        zmsg_destroy(&msg);
        ++nrecv;
        //zsys_debug("recv %d", nrecv);
    }

    zsys_debug("destroy poller");
    zpoller_destroy(&poller);
    zsys_debug("destroy streams");
    for (auto s : streams) {
        zsock_destroy(&s);
    }
    zsys_debug("destroy actor");
    zactor_destroy(&streamer);
    return 0;
}
