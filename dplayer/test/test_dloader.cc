#include "dloader.h"

int main()
{
    zsys_init();
    zsys_set_logident("dloader");

    zactor_t* loader = zactor_new(dloader, NULL);

    const char* address = "inproc://loader";
    zstr_sendx(loader, "BIND", address, NULL);
    zsock_t* reader = zsock_new_pair(address);


    zstr_sendx(loader, "PORT");
    const char* command="";
    int port=-1;
    zsock_recv(loader, "si", &command, &port);
    zsys_info("%s %d", command, port, NULL);
    assert(port==0);            // inproc

    int64_t timeit_beg=0, timeit_end=0;

    // python3 test/test_memmap.py 20000 480
    {
        // LOAD filename (offset, stride, run)
        const int load_args[3] = {0, 480, 480};
        zmsg_t* msg = zmsg_new();
        zmsg_addstr(msg, "LOAD");
        zmsg_addstr(msg, "test_mmap.dat");
        zmsg_addmem(msg, load_args, sizeof(int)*3);
        zmsg_send(&msg, loader);
    }

    {
        // START [delay, nruns]
        timeit_beg = zclock_usecs();
        const int start_args[2] = {1, 1000};
        zmsg_t* msg = zmsg_new();
        zmsg_addstr(msg, "START");
        zmsg_addmem(msg, start_args, sizeof(int)*2);
        zmsg_send(&msg, loader);
    }
    zpoller_t* poller = zpoller_new(zactor_sock(loader), reader, NULL);
    bool done = false;
    int nread = 0;
    while (!done) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("bye bye");
            break;
        }
        if (which == zactor_sock(loader)) {
            zmsg_t *msg = zmsg_recv (which);
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                done = true;
            }
            else if(streq (command, "STOPPED")) {
                timeit_end = zclock_usecs();
                done = true;
                zsys_info("got STOPPED after %f s", 1.0e-6*(timeit_end-timeit_beg));
                
            }
            else {
                zsys_info("unhandled command: %s", command, NULL);
            }
            free(command);
            zmsg_destroy(&msg);
            continue;
        }
        else if (which == reader) {
            // spin fast as there's likely more to read
            bool keep_going = true;
            while (keep_going and zsock_events(reader) & ZMQ_POLLIN) {
                zmsg_t* msg = zmsg_recv(reader);
                ++nread;
                if (nread%500000 == 0) {
                    zsys_info("read %d", nread);
                    keep_going = false;
                }
                zmsg_destroy(&msg);
            }
        }
        else {
            zsys_error("unexpected socket: 0x%x", which, NULL);
            return 1;
        }
    }
    zactor_destroy(&loader);
    zsock_destroy(&reader);
    zpoller_destroy(&poller);

    return 0;
}


