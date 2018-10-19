
#include <czmq.h>

const int nelements = 50000;
const int nreport = nelements*10;
const int nsend = nreport*10;

// this leaks memory.  I must not understand how it works.
//#define ZERO_COPY 1

void fastpair(zsock_t* pipe, void* vargs)
{
    zsock_set_unbounded (pipe);

    zsock_signal(pipe, 0);      // ready    

    zsys_info("looping");
    int nread=0;
    //zpoller_t* poller = zpoller_new(pipe, NULL);
    while (nread < nsend) {
        while (zsock_events(pipe) & ZMQ_POLLIN) {
            zmsg_t* msg = zmsg_recv(pipe);
            if (!msg) {
                zsys_warning("got empty message from pipe");
                continue;
            }
            ++nread;
            //zsys_info("nread: %d", nread);
            if (nread % nreport == 0) {
                zsys_info("read %d", nread);
            }
            zmsg_destroy(&msg);
        }
    }
}



void no_free (void *data, void *hint) {
    // Don't free as data should be from static allocation.
}

int main()
{
    zsys_init();
    zsys_set_logident("fastpair");

    int nsent = 0;
    short int data[nelements] = {0};
    const size_t data_size = nelements*sizeof(short int);

    zactor_t* fpactor = zactor_new(fastpair, NULL);
    assert(fpactor);
    zsock_set_unbounded (fpactor);

    int64_t timeit_beg=0, timeit_end=0;

    timeit_beg = zclock_usecs();

    while (nsent < nsend) {

#if ZERO_COPY
        //  Send message from buffer, which we allocate and ZeroMQ will free for us
        zmq_msg_t message;
        zmq_msg_init_data (&message, data, data_size, no_free, NULL);
        int rc = zmq_sendmsg (zsock_resolve(zactor_sock(fpactor)), &message, 0);
        zmq_msg_close(&message);
#else
        zmsg_t* msg = zmsg_new();
        zframe_t* frame = zframe_new(NULL, data_size);
        memcpy(zframe_data(frame), data, data_size);
        zmsg_append(msg, &frame);
        int rc = zmsg_send(&msg, fpactor);
        assert (rc == 0);

        ++nsent;
        if (nsent % nreport == 0) {
            zsys_info("sent %d", nsent);
        }
#endif
    }
    timeit_end = zclock_usecs();

    zactor_destroy(&fpactor);

    const double dt = 1.0e-6*(timeit_end - timeit_beg);
    const double gbytes_sent = (1.0e-9 * data_size) * nsend;
    zsys_info("summary: %.2f GB %.2fs %f GB/s", gbytes_sent, dt, gbytes_sent/dt);



    return 0;
}
