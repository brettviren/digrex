#include "dexnet/toy.h"
#include "pb/toy.pb.h"
#include "helpers.h"

#include <sstream>
#include <iostream>


using namespace dexnet::toy;

// place holder for logging system
void log(const std::string& msg)
{
    std::cerr << msg << std::endl;
}

zsock_t* tpsource::make_sock(const tpsource::sockdesc& sd)
{
    std::stringstream ss;
    ss << "make sock of type " << sd.type
       << " addr:\"" << sd.addr << "\" bind:" << sd.bind;
    log(ss.str());
    zsock_t* sock = zsock_new(sd.type);
    assert (sock);
    if (sd.addr.empty()) {
        return sock;
    }

    if (sd.bind) {
        const int port = zsock_bind(sock, sd.addr.c_str(), NULL);
        assert(port >= 0);
    }
    else {
        const int rc = zsock_connect(sock, sd.addr.c_str(), NULL);
        assert (rc >= 0);
    }
    return sock;    
}

// A toy source of published trigger primitives in the form of an
// zactor.
// 
void tpsource::actor(zsock_t* pipe, void* vargs)
{
    const auto* cfg = static_cast<const tpsource::config*>(vargs);
    assert(cfg);

    zsock_t* outbox = make_sock(cfg->outbox);

    zsock_t* inbox = make_sock(cfg->inbox);

    zsock_signal (pipe, 0);     // ready

    zpoller_t* poller = zpoller_new(pipe, inbox, NULL);
    assert(poller);
    bool done = false;
    while (!done) {
        log("polling");
        void* which = zpoller_wait(poller, -1);

        if (which == pipe) {    // command
            log("pipe");
            zmsg_t* msg = zmsg_recv(which);
            if (!msg) {
                log("failed to get message from pipe");
                break;
            }

            const std::string command = helpers::popstr(msg);
            if (command == "$TERM") {
                log("tpsource: interupted, shutting down");
                done = true;
            }
            if (command == "LINK") { // connect input (addr, type, typeargs)
                const std::string box = helpers::popstr(msg);
                const std::string endpoint = helpers::popstr(msg);
                const std::string how = helpers::popstr(msg);
                log("command: " + command + " " + box + " " + endpoint + " via " + how);

                if (box == "INBOX") {
                    zsock_connect(inbox, endpoint.c_str(), NULL);
                }
                else if (box == "OUTBOX") {
                    zsock_connect(outbox, endpoint.c_str(), NULL);
                }
                else {
                    log("unknown connect box: " + box + " (" + endpoint +")");
                }
            }
            if (command == "QUIT") {
                log("quitting");
                done = true;
            }
            // more commands....

            zmsg_destroy(&msg);
        }

        if (which == inbox) {
            zmsg_t* msg = zmsg_recv(which);
            const std::string command = helpers::popstr(msg);
            log("got inbox: " + command);
            zmsg_destroy(&msg);
        }


    }
    zsock_destroy(&inbox);
    zsock_destroy(&outbox);
    zpoller_destroy(&poller);
}
