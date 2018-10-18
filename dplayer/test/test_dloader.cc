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

    // python3 test/test_memmap.py 20000 480
    {
        const int load_args[3] = {0, 480, 480};
        zmsg_t* msg = zmsg_new();
        zmsg_addstr(msg, "LOAD");
        zmsg_addstr(msg, "test_mmap.dat");
        zmsg_addmem(msg, load_args, sizeof(int)*3);
        zmsg_send(&msg, loader);
    }

    {
        const int start_args[2] = {1, 2000};
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
                done = true;
                zsys_info("got STOPPED");
            }
            else {
                zsys_info("unknown command: %s", command, NULL);
            }
            free(command);
            zmsg_destroy(&msg);
            continue;
        }
        else if (which == reader) {
            zmsg_t* msg = zmsg_recv(reader);
            ++nread;
            if (nread%100000 == 0) {
                zsys_info("read %d", nread);
            }
            zmsg_destroy(&msg);
        }
        else {
            zsys_error("unexpected socket: 0x%x", which, NULL);
            return 1;
        }
    }
            
    zsys_info("read %d", nread);
    zsys_info("destroy poller");
    zpoller_destroy(&poller);
    zsys_info("destroy reader");
    zsock_destroy(&reader);
    zsys_info("destroy loader");
    zactor_destroy(&loader);
    return 0;
}


