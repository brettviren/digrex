// The data puller is like dloader/dstreamer but sends data out a PUSH socket.
// This actor also eschews a state machine, just to see what that is like.

#include "dexnet/dpuller.h"
#include "dexnet/protohelpers.h"
#include "pb/dpuller.pb.h"
#include "pb/dribbon.pb.h"

#include <czmq.h>

namespace dp = dexnet::dpuller;
namespace dh = dexnet::helpers;
namespace dr = dexnet::dribbon;


struct Context {
    zsock_t *pipe, *src;
    zloop_t* loop;
    size_t nrecv{0};
    int id{-1};
};


static int handle_pipe(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    Context& ctx = *(Context*)vobj;

    zmsg_t* msg = zmsg_recv(sock);
    zframe_t* fid = zmsg_pop(msg);
    int id = dh::msg_id(fid);
    if (!id ) {
        zsys_debug("dpuller: #%d got $TERM after %d recv", ctx.id, ctx.nrecv);
        return -1;
    }
    if (id == dh::msg_id<dp::Connect>()) {
        zframe_t* frame = zmsg_pop(msg);
        dp::Connect c;
        dh::read_frame(frame, c);
        zframe_destroy(&frame);
        int rc = zsock_connect(ctx.src, c.endpoint().c_str(), NULL);
        assert (rc == 0);
    }
    else {
        zsys_warning("dpuller: unknown message ID on pipe: %d", id);
    }
    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return 0;
}


static int handle_src(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    Context& ctx = *(Context*)vobj;
    while (zsock_events(sock) & ZMQ_POLLIN) {
        zmsg_t* msg = zmsg_recv(sock);
        zframe_t* fid = zmsg_pop(msg);
        int id = dh::msg_id(fid);
        zframe_destroy(&fid);
        if (!id ) {
            zsys_debug("dpuller: got $TERM");
            return -1;
        }
        if (id == dh::msg_id<dr::Slice>()) {
            dr::Slice slice;
            zframe_t* frame = zmsg_pop(msg);
            dh::read_frame(frame, slice);
            zframe_destroy(&frame);
            frame = zmsg_pop(msg); // paylaod
            void* vdata = zframe_data(frame);
            size_t vsize = zframe_size(frame);
            // use payload frame here.......
            zframe_destroy(&frame);
            //zsys_debug("slice #%d.%d x %d", slice.sequence(), slice.index(), slice.span_size());
            ++ctx.nrecv;
        }
        else {
            zsys_warning("dpuller: unknown message type %d", id);
        }
        zmsg_destroy(&msg);
    }

    return 0;
}


void dp::actor(zsock_t* pipe, void* vargs)
{
    dp::config* cfg = (dp::config*)vargs;
    Context ctx;
    ctx.pipe = pipe;
    ctx.id = cfg->id;

    ctx.loop = zloop_new();
    ctx.src = zsock_new(ZMQ_PAIR);

    zsock_signal(pipe, 0);      // ready
    zsys_debug("dpuller: ready (#%d)", cfg->id);

    int rc = 0;
    rc = zloop_reader(ctx.loop, ctx.pipe, handle_pipe, &ctx);
    assert (rc == 0);
    rc = zloop_reader(ctx.loop, ctx.src, handle_src, &ctx);
    assert (rc == 0);
    
    zloop_start(ctx.loop);

    zsock_destroy(&ctx.src);
    zloop_destroy(&ctx.loop);
};
    

