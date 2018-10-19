#include "fsm_msm_func.h"
#inlude "fsm_msm_func_custom.h"

// This will eventually be codegen'ed based on msg.jsonnet's.

/*

  

 */


int handle_ctrl(zloop_t* loop, zsock_t* sock, void* vapp)
{
    TppActor* tpp = (TppActor*)vapp;

    // read msg from sock
    // convert to fsm event
    // tpp->process_event()
}


// It is the context for the loop and main fsm.
struct TppActor {
    zsock_t* ctrl;
    zloop_t *loop;

    Tpp(zsock_t* pipe)
        : ctrl(pipe)
        , loop(zloop_new ()) {
    }
    void start() {
        zloop_start(loop);
    }
    void add_socket(zsock_t* sock, zloop_reader_fn handler) {
        int rc = zloop_reader(loop, sock, handler, &app);
        assert (rc == 0);
        zloop_reader_set_tolerant (loop, sock);
    }

};

void tpp(zsock_t* pipe, void* vargs)
{
    TppActor app(pipe, vargs);

    zsock_signal(pipe, 0);      // ready    

    app.add_socket(pipe, handle_ctrl)

}
