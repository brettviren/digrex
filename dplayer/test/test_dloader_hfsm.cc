#include "dloader_hfsm.h"

int main()
{
    zsys_init();
    zsys_set_logident("dloader_hfsm");

    zactor_t* loader = zactor_new(dloader_hfsm, NULL);

    zpoller_t* poller = zpoller_new(zactor_sock(loader), NULL);
    while (true) {
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("bye bye");
            break;
        }
        if (which == zactor_sock(loader)) {
            zsys_info("loader");
            zmsg_t *msg = zmsg_recv (which);
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                break;
            }
        }
    }
    zactor_destroy(&loader);
    zpoller_destroy(&poller);
    return 0;
}
