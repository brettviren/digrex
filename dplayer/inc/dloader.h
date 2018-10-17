#ifndef dplayer_dloader_h_seen
#define dplayer_dloader_h_seen


#include <czmq.h>

struct dloader_cfg_t {
    const char* endpoint = 0;
};
void dloader(zsock_t* pipe, void* vargs);


#endif
