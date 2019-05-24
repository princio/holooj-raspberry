#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "ny2.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char byte;


typedef enum {
	GenericFile
} GenericError;

typedef enum {
        SOCreation,
        SOBind,
        SOListening,
        SOAccept,
        SOSend,
        SORecv,
        SOAddr
} SOError;

typedef enum {
        PollGeneric,
        PollTimeout,
        PollRecvBusy,
        PollIn,
        PollOut
} PollError;

typedef enum {
        ImWrongSize,
        ImUnsopportedColorSpace
} ImError;


int socket_start_server(const char *iface, unsigned int port);
int socket_wait_connection();
int socket_send(byte *cbuf, size_t l, int flags);
int socket_recv(byte *cbuf, size_t l, int flags);
int socket_read_close();
int socket_server_close();
int socket_close();


#ifdef __cplusplus
}
#endif
#endif