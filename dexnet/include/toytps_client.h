/*  =========================================================================
    toytps_client - Toy Trigger Primitive Source Client

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: toytps_client.xml, or
     * The code generation script that built this file: zproto_client_c
    ************************************************************************
    =========================================================================
*/

#ifndef TOYTPS_CLIENT_H_INCLUDED
#define TOYTPS_CLIENT_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef TOYTPS_CLIENT_T_DEFINED
typedef struct _toytps_client_t toytps_client_t;
#define TOYTPS_CLIENT_T_DEFINED
#endif

//  @interface
//  Create a new toytps_client, return the reference if successful, or NULL
//  if construction failed due to lack of available memory.
toytps_client_t *
    toytps_client_new (void);

//  Destroy the toytps_client and free all memory used by the object.
void
    toytps_client_destroy (toytps_client_t **self_p);

//  Return actor, when caller wants to work with multiple actors and/or
//  input sockets asynchronously.
zactor_t *
    toytps_client_actor (toytps_client_t *self);

//  Return message pipe for asynchronous message I/O. In the high-volume case,
//  we send methods and get replies to the actor, in a synchronous manner, and
//  we send/recv high volume message data to a second pipe, the msgpipe. In
//  the low-volume case we can do everything over the actor pipe, if traffic
//  is never ambiguous.
zsock_t *
    toytps_client_msgpipe (toytps_client_t *self);

//  Return true if client is currently connected, else false. Note that the
//  client will automatically re-connect if the server dies and restarts after
//  a successful first connection.
bool
    toytps_client_connected (toytps_client_t *self);

//  Enable verbose tracing (animation) of state machine activity.
void
    toytps_client_set_verbose (toytps_client_t *self, bool verbose);

//  Self test of this class
void
    toytps_client_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
