/*  =========================================================================
    toytps_msg - toy tps msg protocol

    Codec header for toytps_msg.

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: toytps_msg.xml, or
     * The code generation script that built this file: zproto_codec_c
    ************************************************************************
    =========================================================================
*/

#ifndef TOYTPS_MSG_H_INCLUDED
#define TOYTPS_MSG_H_INCLUDED

/*  These are the toytps_msg messages:

    LINK -
        box                 string      Name of INBOX or OUTBOX
        endpoint            string      ZeroMQ endpoint string
        bind                number 1    Set 1 to bind 0 to connect

    UNLINK -
        box                 string      Name of INBOX or OUTBOX
        endpoint            string      ZeroMQ endpoint string
        bind                number 1    Set 1 to bind 0 to connect
*/


#define TOYTPS_MSG_LINK                     1
#define TOYTPS_MSG_UNLINK                   2

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  Opaque class structure
#ifndef TOYTPS_MSG_T_DEFINED
typedef struct _toytps_msg_t toytps_msg_t;
#define TOYTPS_MSG_T_DEFINED
#endif

//  @interface
//  Create a new empty toytps_msg
toytps_msg_t *
    toytps_msg_new (void);

//  Create a new toytps_msg from zpl/zconfig_t *
toytps_msg_t *
    toytps_msg_new_zpl (zconfig_t *config);

//  Destroy a toytps_msg instance
void
    toytps_msg_destroy (toytps_msg_t **self_p);

//  Create a deep copy of a toytps_msg instance
toytps_msg_t *
    toytps_msg_dup (toytps_msg_t *other);

//  Receive a toytps_msg from the socket. Returns 0 if OK, -1 if
//  the read was interrupted, or -2 if the message is malformed.
//  Blocks if there is no message waiting.
int
    toytps_msg_recv (toytps_msg_t *self, zsock_t *input);

//  Send the toytps_msg to the output socket, does not destroy it
int
    toytps_msg_send (toytps_msg_t *self, zsock_t *output);


//  Print contents of message to stdout
void
    toytps_msg_print (toytps_msg_t *self);

//  Export class as zconfig_t*. Caller is responsibe for destroying the instance
zconfig_t *
    toytps_msg_zpl (toytps_msg_t *self, zconfig_t* parent);

//  Get/set the message routing id
zframe_t *
    toytps_msg_routing_id (toytps_msg_t *self);
void
    toytps_msg_set_routing_id (toytps_msg_t *self, zframe_t *routing_id);

//  Get the toytps_msg id and printable command
int
    toytps_msg_id (toytps_msg_t *self);
void
    toytps_msg_set_id (toytps_msg_t *self, int id);
const char *
    toytps_msg_command (toytps_msg_t *self);

//  Get/set the box field
const char *
    toytps_msg_box (toytps_msg_t *self);
void
    toytps_msg_set_box (toytps_msg_t *self, const char *value);

//  Get/set the endpoint field
const char *
    toytps_msg_endpoint (toytps_msg_t *self);
void
    toytps_msg_set_endpoint (toytps_msg_t *self, const char *value);

//  Get/set the bind field
byte
    toytps_msg_bind (toytps_msg_t *self);
void
    toytps_msg_set_bind (toytps_msg_t *self, byte bind);

//  Self test of this class
void
    toytps_msg_test (bool verbose);
//  @end

//  For backwards compatibility with old codecs
#define toytps_msg_dump     toytps_msg_print

#ifdef __cplusplus
}
#endif

#endif
