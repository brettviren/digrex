// The data pusher is like dloader/dstreamer but sends data out a PUSH socket.
// This actor also eschews a state machine, just to see what that is like.

#include "dexnet/dpusher.h"
#include "dexnet/protohelpers.h"
#include "pb/dpusher.pb.h"
#include "pb/dribbon.pb.h"

#include <czmq.h>

namespace dp = dexnet::dpusher;
namespace dh = dexnet::helpers;
namespace dr = dexnet::dribbon;


struct Context {
    zsock_t *pipe, *src, *dst;
    zloop_t* loop;
    int port;
    int nsplit{0};
    int nsent{0};
};


static int handle_pipe(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    Context& ctx = *(Context*)vobj;

    zmsg_t* msg = zmsg_recv(sock);
    zframe_t* fid = zmsg_pop(msg);
    int id = dh::msg_id(fid);
    if (id == dh::msg_id<dp::AskPort>()) {
        dp::Port p; p.set_port(ctx.port);
        zmsg_t* ret = dh::make_msg(p);
        int rc = zmsg_send(&ret, sock);
        assert (rc == 0);
    }
    else if (id == dh::msg_id<dp::Connect>()) {
        zframe_t* frame = zmsg_pop(msg);
        dp::Connect c;
        dh::read_frame(frame, c);
        zframe_destroy(&frame);
        int rc = zsock_connect(ctx.src, c.endpoint().c_str(), NULL);
        assert (rc == 0);
        ctx.nsplit = c.nsplit();
    }
    else {
        zsys_warning("unknown message ID on pipe: %d", id);
    }
    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return 0;
}

static int send_data(Context& ctx, dr::Slice& slice, void* vdata, size_t vsize)
{
    const size_t stride = slice.span_size();
    const size_t nticks = slice.nticks();
    if (vsize != stride*nticks*sizeof(dp::sample_t)) {
        zsys_warning("corrupted data");
        return 0;
    }

    dp::sample_t* data = (dp::sample_t*) vdata;

    const size_t nper = stride/ctx.nsplit; // fixme what about remainder?

    /*
      eg:
      split indices = [0,1,2,3], nsplit=4
      nper = #channels split
      ch:0 1 2 3 4 5 6 7 8 9 10 11 12 13 14....
      |--------+--------+--------+--------|
      | split0 | split1 | split2 | split3 |
      |--------+--------+--------+--------|
      | tick   |        |        |        |
      | tick   |        |        |        |
      | tick   |        |        |        |
      |--------+--------+--------+--------|
     */
    for (size_t isplit=0; isplit<ctx.nsplit; ++isplit) {
        dr::Slice slout;
        slout.set_sequence(ctx.nsent);
        slout.set_index(isplit);
        for (size_t ich = 0; ich < nper; ++ich) {
            slout.set_span(ich, ich + isplit*nper);
        }
        slout.set_nticks(nticks);
        slout.set_overlap(0);

        zmsg_t* msg = dh::make_msg(slout);
        zframe_t* payload = zframe_new(NULL, nticks*nper*sizeof(dp::sample_t));
        dp::sample_t* zdat = (dp::sample_t*)zframe_data(payload);

        for (int itick=0; itick<nticks; ++itick) {
            size_t off_in  = itick*stride + isplit*nper;
            size_t off_out = itick*nper;
            memcpy(zdat + off_out, data + off_in, nper*sizeof(dp::sample_t));
        }
        zmsg_append(msg, &payload);
        int rc = zmsg_send(&msg, ctx.dst);
        assert(rc==0);
    }
    ++ctx.nsent;
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
        if (id == dh::msg_id<dr::Slice>()) {
            dr::Slice slice;
            zframe_t* frame = zmsg_pop(msg);
            dh::read_frame(frame, slice);
            zframe_destroy(&frame);
            frame = zmsg_pop(msg); // paylaod
            void* vdata = zframe_data(frame);
            size_t vsize = zframe_size(frame);
            send_data(ctx, slice, vdata, vsize);
            zframe_destroy(&frame);
        }
        else {
            zsys_warning("unknown message type %d", id);
        }
        zmsg_destroy(&msg);
    }

    return 0;
}

static int handle_dst(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    Context& ctx = *(Context*)vobj;
    zsys_warning("Got unexpected back talk from destination port");
    return -1;
}



void dp::actor(zsock_t* pipe, void* vargs)
{
    dp::config* cfg = (dp::config*)vargs;
    Context ctx;
    ctx.pipe = pipe;

    ctx.loop = zloop_new();
    ctx.src = zsock_new(ZMQ_PAIR);
    ctx.dst = zsock_new(ZMQ_PUSH);
    ctx.port = zsock_bind(ctx.dst, cfg->endpoint, NULL);

    zsock_signal(pipe, 0);      // ready

    int rc = 0;
    rc = zloop_reader(ctx.loop, ctx.pipe, handle_pipe, &ctx);
    assert (rc == 0);
    rc = zloop_reader(ctx.loop, ctx.src, handle_src, &ctx);
    assert (rc == 0);
    rc = zloop_reader(ctx.loop, ctx.dst, handle_dst, &ctx);
    assert (rc == 0);
    
    zloop_start(ctx.loop);

    zsock_destroy(&ctx.dst);
    zsock_destroy(&ctx.src);
    zloop_destroy(&ctx.loop);
};
    

