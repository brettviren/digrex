/*  =========================================================================
    toytps_server - Toy Trigger Primitive Source Service

    ** WARNING *************************************************************
    THIS SOURCE FILE IS 100% GENERATED. If you edit this file, you will lose
    your changes at the next build cycle. This is great for temporary printf
    statements. DO NOT MAKE ANY CHANGES YOU WISH TO KEEP. The correct places
    for commits are:

     * The XML model used for this code generation: toytps_server.xml, or
     * The code generation script that built this file: zproto_server_c
    ************************************************************************
    =========================================================================
*/

#ifndef TOYTPS_SERVER_H_INCLUDED
#define TOYTPS_SERVER_H_INCLUDED

#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  To work with toytps_server, use the CZMQ zactor API:
//
//  Create new toytps_server instance, passing logging prefix:
//
//      zactor_t *toytps_server = zactor_new (toytps_server, "myname");
//
//  Destroy toytps_server instance
//
//      zactor_destroy (&toytps_server);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (toytps_server, "VERBOSE");
//
//  Bind toytps_server to specified endpoint. TCP endpoints may specify
//  the port number as "*" to acquire an ephemeral port:
//
//      zstr_sendx (toytps_server, "BIND", endpoint, NULL);
//
//  Return assigned port number, specifically when BIND was done using an
//  an ephemeral port:
//
//      zstr_sendx (toytps_server, "PORT", NULL);
//      char *command, *port_str;
//      zstr_recvx (toytps_server, &command, &port_str, NULL);
//      assert (streq (command, "PORT"));
//
//  Specify configuration file to load, overwriting any previous loaded
//  configuration file or options:
//
//      zstr_sendx (toytps_server, "LOAD", filename, NULL);
//
//  Set configuration path value:
//
//      zstr_sendx (toytps_server, "SET", path, value, NULL);
//
//  Save configuration data to config file on disk:
//
//      zstr_sendx (toytps_server, "SAVE", filename, NULL);
//
//  Send zmsg_t instance to toytps_server:
//
//      zactor_send (toytps_server, &msg);
//
//  Receive zmsg_t instance from toytps_server:
//
//      zmsg_t *msg = zactor_recv (toytps_server);
//
//  This is the toytps_server constructor as a zactor_fn:
//
void
    toytps_server (zsock_t *pipe, void *args);

//  Self test of this class
void
    toytps_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
