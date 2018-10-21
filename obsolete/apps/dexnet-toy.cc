#include "dexnet/tpsource.h"
#include <iostream>

int main(int argc, char* argv[])
{
    const char* inaddr = "inproc://clock";
    const char* outaddr = "tcp://127.0.0.1:5469";

    zsock_t* ticks = zsock_new(ZMQ_PAIR);
    zsock_bind(ticks, inaddr, NULL);
    assert(ticks);

    zsock_t* sub = zsock_new_sub(outaddr, "");
    assert(sub);
    
    zactor_t* tpsource = zactor_new(dexnet::tpsource::actor, NULL);
    zstr_sendx(tpsource, "LINK", "INBOX", inaddr, "CONNECT", NULL);
    zstr_sendx(tpsource, "LINK", "OUTBOX", outaddr, "BIND", NULL);

    std::cerr << "sending tick\n";

    zstr_sendx(ticks, "tick", NULL);

    std::cerr << "sending quit\n";

    zstr_sendx(tpsource, "QUIT", NULL);

    zactor_destroy(&tpsource);
    zsock_destroy(&sub);
    zsock_destroy(&ticks);

    return 0;
}
