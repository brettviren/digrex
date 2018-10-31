#include "dexnet/dfilesrc.h"
#include "dexnet/protohelpers.h"
#include "pb/dribbon.pb.h"
#include "pb/dfilesrc.pb.h"

#include <czmq.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace dr = dexnet::dribbon;
namespace df = dexnet::dfilesrc;
namespace dh = dexnet::helpers;

struct Context {
    zsock_t* pipe, *dst;
    zloop_t* loop;
    int timer_id{-1};
    int port{-1};
    size_t vsize{0};
    void* vdata{NULL};
    df::sample_t *data{NULL};
    size_t stride{0}, nstrides{0}, nsends{0}, nblocks{0};
    size_t nsent{0};            // count sent
    size_t bcursor{0};          // send cursor, loops [0, nblocks)
    size_t ncycles{0};
};

static int handle_timer(zloop_t* loop, int timer_id, void* vobj)
{
    Context& ctx = *(Context*)vobj;

    bool done = false;
    bool exhausted = false;

    if (ctx.bcursor >= ctx.nblocks) { // at end of data
        zsys_debug("dfilesrc: reaches EOF");

        if (ctx.nsends == 0) {    // user wants to loop to end of data
            done = true;
        }
        exhausted = true;
        ctx.bcursor = 0;
    }

    if (ctx.nsent >= ctx.nsends) {       // reached end of requested number of sends
        zsys_debug("dfilesrc: reaches nsends=%d", ctx.nsends);
        exhausted = true;
        done = true;
    }

    if (exhausted) {
        ++ctx.ncycles;
        df::Exhausted ex;
        ex.set_sequence(ctx.ncycles);
        ex.set_sent(ctx.nsent);
        ex.set_finished(done);
        dh::send_msg(ex, ctx.pipe);
        zsys_debug("dfilesrc: exhausted");
    }

    if (done) {
        zloop_timer_end(ctx.loop, timer_id);
        ctx.timer_id = -1;
        return 0;
    }

    // o.w. rock and roll

    dr::Slice slice;
    slice.set_sequence(ctx.nsent);
    for (int ind=0; ind<ctx.stride; ++ind) {
        slice.add_span(ind);
    }
    //zsys_debug("filesrc: slice spans: %d", slice.span_size());
    slice.set_nticks(ctx.nstrides);
    slice.set_overlap(0);
    slice.set_index(-1);

    zmsg_t* msg = dh::make_msg(slice);
    const size_t nsamples = ctx.nstrides*ctx.stride;
    const size_t bsiz = nsamples*sizeof(df::sample_t);
    zframe_t* fblock = zframe_new(NULL, bsiz);
    memcpy(zframe_data(fblock), ctx.data + ctx.bcursor*nsamples, bsiz);
    zmsg_append(msg, &fblock);

    int rc = zmsg_send(&msg, ctx.dst);
    assert(rc == 0);
    ++ctx.nsent;
    ++ctx.bcursor;

}

static int handle_dst(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    zsys_warning("dfilesrc: got back talk from destination");
    zmsg_t* msg = zmsg_recv(sock);
    zmsg_destroy(&msg);
    return 0;
}

static int handle_pipe(zloop_t */*loop*/, zsock_t *sock, void *vobj)
{
    Context& ctx = *(Context*)vobj;

    zmsg_t* msg = zmsg_recv(sock);
    bool term = dh::msg_term(msg);
    zframe_t* fid = zmsg_pop(msg);
    int id = dh::msg_id(fid);
    if (!id) {
        zsys_debug("dfilesrc: got $TERM after %d sends", ctx.nsends);
        assert(term);
        return -1;
    }


    if (id == dh::msg_id<df::AskPort>()) {
        df::Port p; p.set_port(ctx.port);
        zmsg_t* ret = dh::make_msg(p);
        int rc = zmsg_send(&ret, sock);
        assert (rc == 0);
    }
    else if (id == dh::msg_id<df::Load>()) {
        if (ctx.timer_id < 0) {
            zframe_t* frame = zmsg_pop(msg);
            df::Load lobj;
            dh::read_frame(frame, lobj);
            zframe_destroy(&frame);
            int fd = open(lobj.filename().c_str(), O_RDONLY, 0);
            if (fd < 0) {
                zsys_error("dfilesrc: failed with %d to open %s", fd, lobj.filename().c_str());
                assert(fd >= 0);
            }
            struct stat st;
            int rc = fstat(fd, &st);
            assert (rc == 0);
            ctx.vsize = st.st_size;
            ctx.vdata = mmap(NULL, ctx.vsize, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
            ctx.data = lobj.offset() + (df::sample_t*)ctx.vdata;
            size_t nsamples = ctx.vsize / sizeof(df::sample_t) - lobj.offset();
            ctx.stride = lobj.stride();
            ctx.nstrides = lobj.nstrides();
            const size_t nblocksize = ctx.stride * ctx.nstrides;
            const size_t fringe = nsamples%nblocksize;
            if (fringe) {
                nsamples -= fringe; // align to integral number of strides.
            }
            ctx.nblocks = nsamples/nblocksize;

            ctx.nsends = lobj.nsends();
            ctx.nsent = 0;
            ctx.timer_id = zloop_timer(ctx.loop, lobj.delay(), 0, handle_timer, &ctx);
        }
        else {
            zsys_warning("dfilesrc: file already loaded");
        }
    }
    else {
        zsys_warning("dfilesrc: unknown message ID on pipe: 0x%x", id);
    }
    zframe_destroy(&fid);
    zmsg_destroy(&msg);
    return 0;
}




void df::actor(zsock_t* pipe, void* vargs)
{
    df::config* cfg = (df::config*)vargs;
    Context ctx;

    ctx.pipe = pipe;
    ctx.dst = zsock_new(ZMQ_PAIR);
    ctx.port = zsock_bind(ctx.dst, cfg->endpoint, NULL);
    ctx.loop = zloop_new();

    zsock_signal(pipe, 0);      // ready
    zsys_debug("dfilesrc: ready");

    int rc = 0;
    rc = zloop_reader(ctx.loop, ctx.pipe, handle_pipe, &ctx);
    assert (rc == 0);
    rc = zloop_reader(ctx.loop, ctx.dst, handle_dst, &ctx);
    assert (rc == 0);

    
    zloop_start(ctx.loop);

    zsock_destroy(&ctx.dst);
    zloop_destroy(&ctx.loop);
}
