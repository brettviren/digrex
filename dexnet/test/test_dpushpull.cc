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

    df::config dfcfg{"inproc://fullstream"};
    zactor_t* loader = zactor_new(df::actor, (void*)&dfcfg);

    ds::config dscfg{"inproc://splitstream"};
    zactor_t* pusher = zactor_new(ds::actor, (void*)&dscfg);

    dl::config dlcfg{};
    zactor_t* puller = zactor_new(dl::actor, (void*)&dlcfg);

    {
        df::Load cmd;
        cmd.set_filename(datafile);
        cmd.set_offset(0);
        cmd.set_stride(stride);
        cmd.set_delay(delay);
        cmd.set_nstrides(nstrides);
        cmd.set_nsends(nsends);
        rc = dh::send_msg(cmd, zactor_sock(loader));
        assert(rc == 0);
    }
    
    {
        ds::Connect cmd;
        cmd.set_endpoint(dfcfg.endpoint);
        cmd.set_nsplit(nstreams);
        rc = dh::send_msg(cmd, zactor_sock(pusher));
        assert(rc == 0);
    }

    {
        dl::Connect cmd;
        cmd.set_endpoint(dscfg.endpoint);
        rc = dh::send_msg(cmd, zactor_sock(puller));
        assert(rc == 0);
    }

    zpoller_t* poller = zpoller_new(loader, pusher, puller, NULL);
    while (true) {
        void *which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("test_dpushpull interupted");
            break;
        }
        zmsg_t* msg = zmsg_recv((zsock_t*)which);
        if (which == zactor_sock(loader)) {
            zsys_debug("back talk from filesrc");
        }
        if (which == zactor_sock(pusher)) {
            zsys_debug("back talk from pusher");            
        }
        if (which == zactor_sock(puller)) {
            zsys_debug("back talk from puller");            
        }
        zmsg_destroy(&msg);
    }

    zactor_destroy(&puller);
    zactor_destroy(&pusher);
    zactor_destroy(&loader);
    zpoller_destroy(&poller);

    return 0;
}

