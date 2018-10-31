#include <czmq.h>
#include <iostream>
#include <string>
using namespace std;

void ep(zsock_t* s, std::string m="");
void ep(zsock_t* s, std::string m)
{
    auto ep = zsock_endpoint(s);
    if (ep) {
        cout << m <<": " << ep << endl;
    }
}
int main ()
{
    zsock_t* s = zsock_new(ZMQ_PAIR);

    const char* addr = "inproc://foo";
    int rc=0;

    // next to return nothing.  
    ep(s, "start");

    rc = zsock_connect(s, addr, NULL);
    assert(rc == 0);
    ep(s, "after connect");

    rc = zsock_disconnect(s, addr, NULL);
    assert(rc == 0);
    ep(s, "after disconnect");

    // both return addr
    rc = zsock_bind(s, addr, NULL);
    assert(rc == 0);
    ep(s, "after bind");

    rc = zsock_unbind(s, addr, NULL);
    assert(rc == 0);
    ep(s, "after unbind");

    return 0;
}

