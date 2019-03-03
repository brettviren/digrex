#include <czmq.h>

int main()
{
    zsys_init();
    zsock_t* pub = zsock_new_pub("tcp://127.0.0.1:12345");
    if (!pub) {
        return -1;
    }
    zclock_sleep(200);
    for (int n=0; n<1000; ++n) {
        char *msg = zsys_sprintf("hello world %d", n);
        int rc = zsock_send(pub, "ss", "logtest.INFO", msg);
        free (msg);
        if (rc != 0) {
            break;
        }
        zsys_info("sending %d", n);
        int64_t before = zclock_usecs()/1000;
        zclock_sleep(1000);
        int64_t after = zclock_usecs()/1000;
        if (std::abs(after-before-1000) > 1) {
            zsys_info("before=%d after=%d diff=%d", before, after, after-before);
            break;
        }
    }

    
    zsock_destroy(&pub);

    return 0;
}
