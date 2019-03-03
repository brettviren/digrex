#include <czmq.h>

int main()
{
    zsock_t* sub = zsock_new_sub("tcp://127.0.0.1:12345", "logtest");

    for (int n=0; n<100; ++n) {
        char *topic=0, *logmsg=0;
        int rc = zsock_recv(sub, "ss", &topic, &logmsg);
        if (rc != 0) {
            break;
        }
        zsys_info("\"%s\" \"%s\"", topic, logmsg);
        free (topic);
        free (logmsg);
    }
   
    zsock_destroy(&sub);

    return 0;
}
