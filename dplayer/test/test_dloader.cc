#include "dloader.h"
#include <iostream>

int main()
{
    zactor_t* loader = zactor_new(dloader, NULL);

    zstr_sendx(loader, "BIND", "inproc://loader", NULL);

    zstr_sendx(loader, "PORT");
    const char* command="";
    int port=-1;
    zsock_recv(loader, "si", &command, &port);
    std::cerr << command << " " << port << std::endl;
    assert(port==0);            // inproc

    zactor_destroy(&loader);
    return 0;
}


