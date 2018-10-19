/* a zactor that preloads data into RAM and then spurts it at a fixed rate */


#include "dloader.h"
#include <czmq.h>
#include <string>
#include <vector>
#include <deque>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int handle_outpipe (zloop_t *loop, zsock_t *outpipe, void *dloaderobj);
int handle_inpipe (zloop_t *loop, zsock_t *inpipe, void *dloaderobj);


struct dloader_obj_t {
    zsock_t *inpipe = nullptr;
    zsock_t *outpipe = nullptr;
    int port = -1;
    std::string endpoint = "";
    zloop_t *loop;
    int timer_id = 0;
    bool paused=true;            // must be false to start sending data
    short int *data;
    short int *data_ptr=nullptr; // where we are in the data
    short int *data_end=nullptr;
    size_t data_nbytes;    // size measured in bytes
    int data_offset = 0;        // where in data to start
    int data_stride = 0;        // each data read starts one stride further
    int data_run = 0;           // how much contiguous data to emit
    int data_nruns = 0;
    int data_sent = 0;

    dloader_obj_t(zsock_t* pipe=nullptr)
        : inpipe(pipe)
        , outpipe(zsock_new(ZMQ_PAIR))
        , loop(zloop_new())
    {
        int rc = zloop_reader (loop, outpipe, handle_outpipe, this);
        assert (rc == 0);
        zloop_reader_set_tolerant (loop, outpipe);

        rc = zloop_reader (loop, inpipe, handle_inpipe, this);
        assert (rc == 0);
        zloop_reader_set_tolerant (loop, inpipe);
    }

    ~dloader_obj_t() {
        zsock_destroy (&outpipe); 
        zloop_destroy (&loop);
    }

};

int dloader_load(dloader_obj_t& self, const char* filename, int offset, int stride, int run);
int dloader_start(dloader_obj_t& self, int delay, int nruns);
int dloader_pause(dloader_obj_t& self);
int dloader_stop(dloader_obj_t& self);


int dloader_bind(dloader_obj_t& self, std::string endpoint)
{
    if (self.port >= 0) {                 // already bound, rebind
        zsock_unbind(self.outpipe, self.endpoint.c_str(), NULL);
        self.endpoint="";
        self.port=-1;
    }
    self.port = zsock_bind(self.outpipe, endpoint.c_str(), NULL);
    assert(self.port >= 0);     // inproc gets port=0
    self.endpoint = endpoint;
    return self.port;
}            

int handle_inpipe (zloop_t *loop, zsock_t *inpipe, void *dloaderobj)
{
    dloader_obj_t& self = *(dloader_obj_t*)dloaderobj;

    zmsg_t *msg = zmsg_recv (inpipe);
    if (!msg) {
        return -1;
    }
    char *command = zmsg_popstr (msg);

    int ret = 0;
    if (streq (command, "$TERM")) {
        ret = -1;
    }
    else if (streq (command, "BIND")) {
        char* endpoint = zmsg_popstr(msg);
        int port = dloader_bind(self, endpoint);
        free (endpoint);
        if (port < 0) {
            zsys_error("error binding to \"%s\"", endpoint, NULL);
            ret = -1;
        }
    }
    else if (streq (command, "PORT")) {
        ret = zsock_send(inpipe, "si", "PORT", self.port);
    }
    // START [delay, nruns]
    else if (streq (command, "START")) {
        const int nargs=2;
        zframe_t* frame = zmsg_pop(msg);
        size_t siz = zframe_size(frame);
        assert(siz == sizeof(int)*2);
        int* data = (int*)zframe_data(frame);
        ret = dloader_start(self, data[0], data[1]);
        zframe_destroy(&frame);
    }
    else if (streq (command, "PAUSE")) {
        ret = dloader_pause(self);
    }
    // LOAD filename (offset, stride, run)
    else if (streq (command, "LOAD")) {
        char* filename = zmsg_popstr(msg);
        zframe_t* frame = zmsg_pop(msg);
        size_t siz = zframe_size(frame);
        const int nargs = 3;
        assert(siz == sizeof(int)*nargs);
        int* data = (int*)zframe_data(frame);
        zframe_destroy(&frame);
        ret = dloader_load(self, filename, data[0], data[1], data[2]);
        if (ret < 0) {
            zsys_error("failed to open file %s", filename);
            ret = zsock_send(inpipe, "ss", "LOADFAIL", filename);
        }
        else {
            ret = zsock_send(inpipe, "ss", "LOADOK", filename);
        }
        free (filename);
    }
    else {
        zsys_warning("unknown command: \"%s\"", command, NULL);
        ret = -1;
    }
    
    zsys_info("inpipe: \"%s\" returns %d", command, ret, NULL);

    free (command);
    zmsg_destroy (&msg);

    return ret;
}
void no_free (void *data, void *hint) {
}

int handle_outpipe (zloop_t *loop, zsock_t *outpipe, void *dloaderobj)
{
    // got some input on outpipe
    // read message
    // deal with it
    zsys_warning("got input from outpipe, not yet supported");
    return 0;
}

int handle_send(zloop_t* loop, int timer_id, void* dloaderobj)
{
    zsys_info("send");
    dloader_obj_t& self = *(dloader_obj_t*)dloaderobj;
    if (!self.data) {
        zsys_error("send with no data");
        return -1;
    }
    if (!self.outpipe) {
        zsys_error("send with no outpipe");
        return -1;
    }
    int rc = 0;
    int nruns = 0;
    while (true) {
        if (nruns >= self.data_nruns) {
            zsys_debug("sent %d", self.data_sent, NULL);
            return 0;           // done for this send
        }
        if (self.data_ptr >= self.data_end) {
            dloader_stop(self); // no mo data, yo
            return 0;
        }
        zmsg_t* msg = zmsg_new();
        {
            zframe_t* frame = zframe_new(&self.data_sent, sizeof(int));
            zmsg_append(msg, &frame);
        }
        {
            const size_t nrunbytes = self.data_run * sizeof(short int);
            // zero'ish copy 
            zframe_t* frame = zframe_new(NULL, nrunbytes);
            //memcpy(zframe_data(frame), self.data_ptr, nrunbytes);
            zmsg_append(msg, &frame);
        }
        rc = zmsg_send(&msg, self.outpipe);

        if (rc < 0) {
            zsys_error("failed to send number %d", self.data_sent, NULL);
            break;
        }
        self.data_sent += 1;
        self.data_ptr += self.data_stride;
    }
    return rc;
}

int dloader_start(dloader_obj_t& self, int delay, int nruns)
{
    if (!self.paused) {
        zsys_warning("start already running", NULL);
        return 0;               // already started
    }
    if (self.port < 0) {
        zsys_warning("start not yet bound");
        return -1;
    }
            
    self.data_nruns = nruns;
    self.timer_id = zloop_timer(self.loop, delay, 0, handle_send, (void*)&self);
    zsys_info("starting timer ID %d with delay %d", self.timer_id, delay, NULL);
    return 0;
}
int dloader_stop(dloader_obj_t& self)
{
    // fixme: what if we have any outstanding users of this data
    int rc=0;
    if (self.data) {
#if USE_MMAP
        rc = munmap((void*)self.data, self.data_nbytes);
        assert(rc == 0);
#else
        free((void*)self.data);
#endif
    }
    self.data_nbytes = 0;
    self.data = NULL;
    self.data_ptr = nullptr;
    self.data_end=nullptr;
    self.data_nbytes = 0;    
    self.data_offset = 0;
    self.data_stride = 0;
    self.data_run = 0; 
    self.data_nruns = 0;
    const int sent = self.data_sent;
    self.data_sent = 0;

    rc = zsock_send(self.inpipe, "si", "STOPPED", sent);
    zsys_info("stopping after %d", sent, NULL);
    return rc;
}

int dloader_pause(dloader_obj_t& self)
{
    if (self.paused) {
        return 0;
    }
    return 0;
}

int dloader_load(dloader_obj_t& self, const char* filename, int offset, int stride, int run)
{
    if (self.data_nbytes) {
        int rc = dloader_stop(self);
        if (rc < 0) { return rc; }
    }

    int fd = open(filename, O_RDONLY, 0);
    if (fd<0) {
        return -1;
    }
    struct stat st;
    stat(filename, &st);
#if USE_MMAP
    void* vdata = mmap(NULL, st.st_size,
                     PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
    close(fd);                  // mmap stays available
    if (vdata == MAP_FAILED) {
        return -1;
    }
#else
    void* vdata = malloc(st.st_size);
    int rc = read(fd, vdata, st.st_size);
    if (rc < 0) {
        return -1;
    }
    close(fd);
#endif
        
    self.data = (short int*) vdata;
    self.data_nbytes = st.st_size;
    self.data_ptr = self.data + offset;
    self.data_end = self.data + self.data_nbytes / sizeof(short int); // one past last element.
    self.data_stride = stride;
    self.data_run = run;
    self.data_sent = 0;
    return 0;
}

void dloader(zsock_t* pipe, void* vargs)
{
    dloader_obj_t obj(pipe);
    if (vargs) {
        dloader_cfg_t* cfg = (dloader_cfg_t*)vargs;
        if (cfg->endpoint) {
            dloader_bind(obj, cfg->endpoint);
        }
    }
    
    zsock_signal(pipe, 0);      // ready    

    zloop_start (obj.loop);

}
