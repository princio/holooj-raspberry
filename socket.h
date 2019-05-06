#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "ny2.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned char byte;
typedef const int error;


typedef struct _RecvBuffer
{
    int stx;
    int index;
    int l;
    byte image[];
} RecvBuffer;

typedef struct _SendBuffer
{
    int stx;
    int index;
    int n;
    rec_object dets[5];
} SendBuffer;


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
        PckGeneric,
        PckSTX,
        PckTooSmall
} PckError;

typedef enum {
        TjGeneric,
        TjHeader,
        TjDecompress,
        TjCompress,
        TjDestroy
} TjError;

typedef enum {
        ImWrongSize,
        ImUnsopportedColorSpace
} ImError;


#define RIFE( expr, mask, code, msg, ... )\
    if((expr)) {\
        if(!strcmp(#mask, "SO")) {\
            printf("\n[%s::%d]\t"#mask" "#code"\t(errno#%d=%s)"msg".\n\n", __FILE__, __LINE__, errno, strerror(errno), ##__VA_ARGS__);\
            socket_errno = (mask##Error) mask##code;\
        } else {\
            printf("\n[%s::%d]\t"#mask" "#code"\t"msg".\n\n", __FILE__, __LINE__, ##__VA_ARGS__);\
            socket_errno = (mask##Error) mask##code;\
        }\
	return -1;\
    }

#ifdef __cplusplus
}
#endif
#endif