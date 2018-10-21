#include "dexnet/wfsource.h"

#include "helpers.h"

#include <string>
#include <iostream>
using namespace std;


struct wfsource_app_t {
    zsock_t* pipe;
    helpers::sock_link_t outbox;
    int wakeup;

    bool ready () {
        return outbox.ready();  // no inbox
    }

    int link(const std::string& box,
             const std::string& endpoint,
             const std::string& how)        {
        helpers::sock_link_t* sl = NULL;
        if (box == "INBOX") return -1; // no inbox
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

    wfsource_app_t(zsock_t* pipe)
        : pipe(pipe)
        , outbox(zsock_new(ZMQ_PAIR)) // for now, 
        , wakeup(0)
        {}
};

static int handle_timer(zloop_t *loop, int timer_id, void *vargs)
{
    wfsource_app_t* app = (wfsource_app_t*)vargs;

    // here we pump waveform messages to outbox

    return 0;
}


static int handle_pipe(zloop_t* loop, zsock_t* pipe, void* vargs)
{
    wfsource_app_t* app = (wfsource_app_t*)vargs;

    
    zmsg_t* msg = zmsg_recv(pipe);
    if (!msg) {
        cerr << "failed to get message from pipe\n";
        return -1;
    }

    const std::string command = helpers::popstr(msg);
    if (command == "$TERM") {
        cerr << "wfsource: interupted, shutting down\n";
        return -1;
    }
    if (command == "LINK") { // connect input (addr, type, typeargs)
        const std::string box = helpers::popstr(msg);
        const std::string endpoint = helpers::popstr(msg);
        const std::string how = helpers::popstr(msg);
        app->link(box, endpoint, how);
        return 0;
    }
    if (command == "START") {
        if (app->wakeup) {
            zloop_timer_end(loop, app->wakeup);
            app->wakeup = 0;
        }
        size_t delay = 1;       // ms
        app->wakeup = zloop_timer(loop, delay, 0, handle_timer, app);
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

void dexnet::wfsource::actor(zsock_t* pipe, void* vargs)
{
    wfsource_app_t app(pipe);
    zsock_signal (pipe, 0);     // ready

    zloop_t* loop = zloop_new();
    zloop_reader(loop, pipe, handle_pipe, (void*)&app);
    
    zloop_start(loop);

    zloop_destroy(&loop);
    zsock_destroy(&app.outbox.sock);
}
