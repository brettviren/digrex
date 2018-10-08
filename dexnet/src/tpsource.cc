#include "dexnet/tpsource.h"

#include "helpers.h"

#include <string>
#include <iostream>
using namespace std;


struct tpsource_app_t {
    zsock_t* pipe;
    helpers::sock_link_t inbox, outbox;

    bool ready () {
        return inbox.ready() and outbox.ready();
    }

    int link(const std::string& box,
             const std::string& endpoint,
             const std::string& how)        {
        helpers::sock_link_t* sl = NULL;
        if (box == "INBOX") sl = &inbox;
        else if (box == "OUTBOX") sl = &outbox;
        else return -1;

        if (how == "BIND") {
            sl->port = zsock_bind(sl->sock, endpoint.c_str(), NULL);
            return sl->port;
        }
        if (how == "CONNECT") {
            sl->port = zsock_connect(sl->sock, endpoint.c_str(), NULL);
            return sl->port;
        }
        return -1;
    }


    tpsource_app_t(zsock_t* pipe)
        : pipe(pipe)
        , outbox(zsock_new(ZMQ_PUB)) // for now, 
        , inbox(zsock_new(ZMQ_PAIR)) // hard code type
        {}
};


static int handle_pipe(zloop_t* loop, zsock_t* pipe, void* vargs)
{
    tpsource_app_t* app = (tpsource_app_t*)vargs;

    
    zmsg_t* msg = zmsg_recv(pipe);
    if (!msg) {
        cerr << "failed to get message from pipe\n";
        return -1;
    }

    const std::string command = helpers::popstr(msg);
    if (command == "$TERM") {
        cerr << "tpsource: interupted, shutting down\n";
        return -1;
    }
    if (command == "LINK") { // connect input (addr, type, typeargs)
        const std::string box = helpers::popstr(msg);
        const std::string endpoint = helpers::popstr(msg);
        const std::string how = helpers::popstr(msg);
        app->link(box, endpoint, how);
        return 0;
    }
    if (command == "QUIT") {
        cerr << "quitting\n";
        return -1;
    }
    // more commands....

    zmsg_destroy(&msg);

    // unknown 
    return -1;
}

static int handle_inbox(zloop_t* loop, zsock_t* inbox, void* vargs)
{
    tpsource_app_t* app = (tpsource_app_t*)vargs;

    if (!app->ready()) {
        return -1;
    }

    // slurp as much as there is
    while (zsock_events(inbox) & ZMQ_POLLIN) {
        zmsg_t *msg = zmsg_recv(inbox);
        if (!msg) {
            return -1;
        }
        // what do these messages look like?
        // app->ingest_channels(msg)
    }

    return 0;
}

void dexnet::tpsource::actor(zsock_t* pipe, void* vargs)
{
    tpsource_app_t app(pipe);
    zsock_signal (pipe, 0);     // ready

    zloop_t* tps_loop = zloop_new();
    zloop_reader(tps_loop, pipe, handle_pipe, (void*)&app);
    zloop_reader(tps_loop, app.inbox.sock, handle_inbox, (void*)&app);
    
    zloop_start(tps_loop);

    zloop_destroy(&tps_loop);
    zsock_destroy(&app.inbox.sock);
    zsock_destroy(&app.outbox.sock);
}
