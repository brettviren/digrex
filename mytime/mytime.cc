/*

notes: 

- zero-copy frame https://github.com/mhaberler/directoryd/blob/master/main.cpp#L262

 */

#include <czmq.h>
#include <zyre.h>

#include <map>
#include <string>
#include <sstream>
#include <iostream>

#include "mytime.pb.h"

// TODO: 
// - [ ] write sugar for a send<PB>(sock) and recv<PB>(sock).
// each PB message is two frames: a short string giving the PB type and then the serialized data.
// - [ ] convienient message dispatch idiom, keyed on type.
// - [ ] sorted queue



// Pop next frame and fill protobuf object
template <typename ProtoBufObj>
bool pop_frame_fill(ProtoBufObj& obj, zmsg_t* msg)
{
    zframe_t* frame = zmsg_pop(msg);
    obj.ParseFromArray(zframe_data(frame), zframe_size(frame));
    zframe_destroy(&frame);
    return true;
}

// mediate with world via Zyre
struct comms_args_t {
    const char* name;
    int pub_port;
};
static void start_comms(zsock_t *pipe, void* vargs)
{
    comms_args_t* args = (comms_args_t*)(vargs);

    zyre_t* node = zyre_new(args->name);
    zyre_set_header(node, "Direx-Pub-Port", "%d", args->pub_port);
    //zyre_set_verbose(node);

    auto okchat = zyre_join (node, "CHAT");
    std::cerr << okchat << std::endl;
    auto okcfg = zyre_join (node, "EPOCH");
    std::cerr << okcfg << std::endl;

    zyre_start(node);
    
    zsock_signal(pipe, 0);      // ready    
    
    bool done = false;
    zpoller_t *poller = zpoller_new (pipe, zyre_socket (node), NULL);
    while (!done) {
        void *which = zpoller_wait (poller, -1);
        if (which == pipe) {
            std::cerr << "pipe message\n";
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted

            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                done = true;
            }
            else {
                puts ("E: invalid message to actor");
                assert (false);
            }
            free (command);
            zmsg_destroy (&msg);
            // nothing supported yet.
            continue;
        }
        if (which == zyre_socket(node)) {
            // check for ECR and shutdown
            // send message down pipe

            // fixme: wrap this up
            zmsg_t *msg = zmsg_recv (which);
            char *event = zmsg_popstr (msg);
            char *peer = zmsg_popstr (msg);
            char *name = zmsg_popstr (msg);
            char *group = zmsg_popstr (msg);
            zframe_t* frame = zmsg_pop(msg);

            if (streq (event, "SHOUT")) {
                std::cerr << "SHOUT group " << group << std::endl;
                if (streq (group, "EPOCH")) {
                    std::cerr << "EPOCH\n";
                    EpochChange ecr;
                    int zerr;
                    try {
                        zerr = ecr.ParseFromArray(zframe_data(frame),
                                                  zframe_size(frame)) ? 0 : -1;
                    }
                    catch (google::protobuf::FatalException fe) {
                        std::cerr << "ECR: corrupt\n";
                        zerr = -1;
                    }
                    if (!zerr) {
                        std::cerr << "ECR: "
                                  << ecr.number() << " "
                                  << ecr.period() << " "
                                  << ecr.start() << "\n";

                        zerr = zstr_sendm(pipe, "EPOCH");
                        assert(zerr == 0);
                        zerr = zframe_send(&frame, pipe, 0);
                        assert(zerr == 0);
                        assert(frame == NULL);
                    }
                }
            }

            free (event);
            free (peer);
            free (name);
            free (group);
            zframe_destroy(&frame);
            zmsg_destroy (&msg);

            continue;
        }
    }
    zpoller_destroy (&poller);
    zyre_stop (node);
    zclock_sleep (100);
    zyre_destroy (&node);
}

struct ticks_args_t {
};
static void start_ticks(zsock_t* pipe, void* vargs)
{
    ticks_args_t* args = (ticks_args_t*)vargs;

    zsock_signal(pipe, 0);      // ready    

    while (true) {
        // produce ticks, sleep, repeat
    }
}

class TriggerProcessorBase {
public:
    int operator()(TickData& td);
}

class TriggerApp {
public:
    struct Args {
    };
    TriggerApp(void* vargs, zsock_t* pipe)
        : m_args((Args*)vargs) 
        , m_pipe(pipe)
    {
        m_tick_actor = zactor_new(start_ticks,(void*) new ticks_args_t);
        m_ticks = zactor_sock(m_tick_actor);
        zsock_signal(pipe, 0);      // ready
    }

    void set_epoch(const EpochChange& ecr) {
        
    }

    void read_pipe() {
        zmg_t* msg = zmsg_recv(m_pipe);
        zframe_t* cmdtyp = zmsg_pop(msg);
        if (zframe_streq(first, "ECR")) {
            zframe_t* frame = zmsg_pop(msg);
            EpochChange ecr;
            zerr = ecr.ParseFromArray(zframe_data(frame),
                                      zframe_size(frame)) ? 0 : -1;
            zframe_destory(&frame);
            // maybe need another level of indirection to pick an "epoch algorithm"
            this->set_epoch(ecr);
        }
        zframe_destory(&cmdtype);
        zms_destroy(&msg);
    }

    void read_ticks() {
    }

    bool run() {
        bool done = false;
        zpoller_t* poller = zpoller_new(m_pipe, m_ticks, NULL);
        while (!done) {
            void *which = zpoller_wait(poller, -1);
            if (which == m_pipe) {
                this->read_pipe();
            }
            else if (which == m_ticks) {
                this->read_ticks();
            }
        }
    }

private:
    Args* m_args;
    zsock_t *m_pipe, *m_ticks;
    zactor_t* tick_actor = zactor_new(start_ticks,(void*) new ticks_args_t);

    // ordered epoch number -> trigger processor
    typedef std::map<int, TriggerProcessor> m_epoch_queue;
};

template <typename App>
void start_app(zsock_t* pipe, void* args)
{
    App app(vargs, pipe);
    app.run();
}

struct mainapp_t {
    zsock_t* publish;
    zactor_t* world_actor;
    zactor_t* trigger_actor;
};

static int handle_world(zloop_t* mainloop, zsock_t* world, void* vargs)
{
    mainapp_t* ma = (mainapp_t*)vargs;
    std::cerr << "world spoke\n";
    zmsg_t* msg = zmsg_recv(world);
    assert(msg);
    zframe_t* first = zmsg_first(msg);
    assert(first);
    if (zframe_streq(first, "ECR")) {
        int zerr = zactor_send(ma->trigger_actor, &msg);
        assert (zerr == 0);
    }
    return 0;
}
static int handle_trigger(zloop_t* mainloop, zsock_t* trigger, void* vargs)
{
    mainapp_t* ma = (mainapp_t*)vargs;
    std::cerr << "trigger spoke\n";
    zmsg_t* msg = zmsg_recv(trigger);
    assert(msg);
    zframe_t* first = zmsg_first(msg);
    assert(first);
    if (zframe_streq(first, "PA")) {
        int zerr = zactor_send(ma->publish, &msg);
        assert (zerr == 0);
    }
    return 0;
}

int main(int argc, char* argv[])
{
    const char *name = "mytime";
    if (argc > 1) { name = argv[1]; }
    const char* address = "127.0.0.1";
    if (argc > 2) { address = argv[2]; }
    int port = 12345;
    if (argc > 3) { port = atoi(argv[3]); }

    std::stringstream ss;
    ss << "tcp://" << address << ":" << port;

    mainapp_t app;

//    app.publish = zsock_new_pub(ss.str().c_str());

    // In general, we make N of these trigger actors.
    app.trigger_actor = zactor_new(start_trigger,
                                   (void*) new trigg_args_t);

    app.world_actor = zactor_new(start_comms,
                                 (void*) new comms_args_t{name, port});

    zloop_t* mainloop = zloop_new();
    zloop_reader(mainloop, zactor_sock(app.trigger_actor), handle_trigger, (void*)&app);
    zloop_reader(mainloop, zactor_sock(app.world_actor), handle_world, (void*)&app);
    
    zloop_start(mainloop);
    zloop_destroy(&mainloop);

    return 0;
}
