/* a zactor that preloads data into RAM and then spurts it at a fixed rate */


#include "dloader.h"
#include <czmq.h>
#include <string>
#include <vector>
#include <deque>
#include <iostream>


int handle_outpipe (zloop_t *loop, zsock_t *outpipe, void *dloaderobj);
int handle_inpipe (zloop_t *loop, zsock_t *inpipe, void *dloaderobj);


struct dloader_obj_t {
    zsock_t *inpipe = nullptr;
    zsock_t *outpipe = nullptr;
    int port = -1;
    std::string endpoint = "";
    zloop_t *loop;
    bool pause=true;            // must be false to start sending data

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
            std::cerr << "Bind to " << endpoint << " returned error" << std::endl;
            ret = -1;
        }
    }
    else if (streq (command, "PORT")) {
        zsock_send(inpipe, "si", "PORT", self.port);
    }
    else if (streq (command, "START")) {
        self.pause = false;
    }
    else if (streq (command, "PAUSE")) {
        self.pause = true;
    }
    else if (streq (command, "LOAD")) {
        char* filename = zmsg_popstr(msg);
        ret = dloader_load(self, filename);
        free (filename);
    }
    else {
        std::cerr << "unknown command: " << command << std::endl;
        ret = -1;
    }
    
    std::cout << "inpipe "<<command<<" returns " << ret << std::endl;

    free (command);
    zmsg_destroy (&msg);

    return ret;
}
int handle_outpipe (zloop_t *loop, zsock_t *outpipe, void *dloaderobj)
{
    // got some input on outpipe
    // read message
    // deal with it
    std::cout << "outpipe\n";
    return 0;
}

int dloader_load(dloader_obj_t& self, const char* filename)
{
    if (self.data_size) {
        // fixme: what if we have any outstanding users of this data

        int rc = munmap(self.data, self.data_size);
        assert(rc == 0);
        self.data_size = 0;
        self.data = NULL;
    }

    int fd = open(filename, O_RDONLY, 0);
    if (fd<0) {
        return fd;
    }
    struct stat st;
    stat(filename, &st);
    self.data = mmap(NULL, s.st_size,
                     PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
    if (self.data == MAP_FAILED) {
        return -1;
    }
    self.data_size = st.st_size;
    close(fd);
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
