#include "helpers.h"

std::string helpers::popstr(zmsg_t* msg)
{
    char* cmsg = zmsg_popstr(msg);
    std::string ret = cmsg;
    free (cmsg);
    return ret;
}
