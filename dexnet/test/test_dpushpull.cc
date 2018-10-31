#include "dexnet/dpusher.h"
#include "dexnet/dpuller.h"
#include "dexnet/dfilesrc.h"

#include "dexnet/protohelpers.h"

#include "pb/dfilesrc.pb.h"
#include "pb/dpuller.pb.h"
#include "pb/dpusher.pb.h"

namespace dh = dexnet::helpers;
namespace df = dexnet::dfilesrc;
namespace ds = dexnet::dpusher;
namespace dl = dexnet::dpuller;


namespace dh = dexnet::helpers;

int main(int argc, char* argv[])
{
    zsys_init();
    zsys_set_logident("test_dpushpull");
    
    if (argc < 2) {
        zsys_error("usage: test_dpushpull file.dat");
        return -1;
    }
    const char* datafile = argv[1];
    if (!zsys_file_exists(datafile)) {
        zsys_error("no such file: %s", datafile);
        return -1;
    }

    int rc=0;
    const int delay = 1;        // delay in ms between sends
    const size_t stride = 960;   // strides between start of neighboring chunks
    const size_t nstrides = 2000; // chunks per send
    const size_t nsends = 1000;  // number of sends before quitting
    const size_t nstreams = 4;
    const size_t npullers = 4;

    df::config dfcfg{"inproc://fullstream"};
    zactor_t* filesrc = zactor_new(df::actor, (void*)&dfcfg);

    ds::config dscfg{"inproc://splitstream"};
    zactor_t* pusher = zactor_new(ds::actor, (void*)&dscfg);

    std::vector<zactor_t*> pullers;
    for (int ip = 0; ip < npullers; ++ip) {
        zactor_t* puller = zactor_new(dl::actor, (void*)new dl::config{ip+1}); // we leak the cfg
        pullers.push_back(puller);
    }

    {
        df::Load cmd;
        cmd.set_filename(datafile);
        cmd.set_offset(0);
        cmd.set_stride(stride);
        cmd.set_delay(delay);
        cmd.set_nstrides(nstrides);
        cmd.set_nsends(nsends);
        rc = dh::send_msg(cmd, zactor_sock(filesrc));
        assert(rc == 0);
        zsys_debug("main: sent msg id %d to filesrc (0x%x)", dh::msg_id(cmd), zactor_sock(filesrc));
    }
    
    {
        ds::Connect cmd;
        cmd.set_endpoint(dfcfg.endpoint);
        cmd.set_nsplit(nstreams);
        rc = dh::send_msg(cmd, zactor_sock(pusher));
        assert(rc == 0);
        zsys_debug("main: sent msg id %d to pusher (0x%x)", dh::msg_id(cmd), zactor_sock(pusher));
    }

    {
        dl::Connect cmd;
        cmd.set_endpoint(dscfg.endpoint);
        for (auto puller : pullers) {
            rc = dh::send_msg(cmd, zactor_sock(puller));
            assert(rc == 0);
            zsys_debug("main: sent msg id %d to puller (0x%x)", dh::msg_id(cmd), zactor_sock(puller));
        }
    }

    zpoller_t* poller = zpoller_new(zactor_sock(filesrc), zactor_sock(pusher), NULL);
    for (auto puller : pullers) {
        zpoller_add(poller, zactor_sock(puller));
    }

    bool done=false;
    int countdown = 3;

    while (countdown >= 0) {
        int timeout_ms = -1;
        if (done) {
            timeout_ms = 100;
            zsys_debug("main: countdown %d", countdown);
            --countdown;
        }
        void *which = zpoller_wait(poller, timeout_ms);
        if (!which) {
            zsys_info("main: interupted");
            done = true;
            continue;
        }

        zmsg_t* msg = zmsg_recv((zsock_t*)which);
        zframe_t* fid = zmsg_pop(msg);        
        int id = dh::msg_id(fid);
        zframe_destroy(&fid);
        zsys_debug("main: got msg id %d from 0x%x", id, which);
        
        if (which == zactor_sock(filesrc)) {
            zsys_debug("back talk from filesrc");

            if (id == dh::msg_id<df::Exhausted>()) {
                zframe_t* frame = zmsg_pop(msg);
                df::Exhausted ex;
                dh::read_frame(frame, ex);
                zframe_destroy(&frame);
                zsys_info("file exhausted %d times, %d sent, %s",
                          ex.sequence(), ex.sent(),
                          ex.finished() ? "terminating" : "continuing");
                if (ex.finished()) {
                    // Note: bailing from here very likely leaves
                    // pusher and/or pullers still active.  The
                    // countdown gives them time to finish up.  In a
                    // real system (not this dumb test) actors keep
                    // spinning until given a pipe command to finish
                    // and they notify their patron that they are
                    // ready.
                    done = true;
                }
            }
        }
        else if (which == zactor_sock(pusher)) {
            zsys_debug("back talk from pusher");            
        }

        else {              // must be a puller
            zsys_debug("back talk from puller");            
        }

        zmsg_destroy(&msg);
    }

    zsys_debug("main: shuting down");

    for (auto puller : pullers) {
        zactor_destroy(&puller);
    }
    zactor_destroy(&pusher);
    zactor_destroy(&filesrc);
    zpoller_destroy(&poller);

    return 0;
}

