#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "yolov2.h"
#define NCS

#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned char byte;
typedef const int error;


typedef struct Color {
    byte r;
    byte g;
    byte b;
} Color;

typedef struct detection2{
    box bbox;
    float prob;
    float objectness;
    char name[28];
} detection2;

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
    detection2 dets[5];
} SendBuffer;


int show_image_cv(void* im, const char* name, int, int);
void make_window(char *name, int w, int h);

#define ERROR(M1, V1, M2, V2) extern error M1##M2##Error;

extern error SOError;
extern error PollError;
extern error PckError;
extern error TjError;
extern error ImError;
extern error NCSError;
extern error MovError;


extern error SOCreationError;
extern error SOBindError;
extern error SOListeningError;
extern error SOAcceptError;
extern error SOSendError;
extern error SORecvError;
extern error SOAddrError;

extern error PollGenericError;
extern error PollTimeoutError;
extern error PollRecvBusyError;
extern error PollInError;
extern error PollOutError;

extern error PckErrorError;
extern error PckSTXError;
extern error PckTooSmallError;

extern error TjGenericError;
extern error TjHeaderError;
extern error TjDecompressError;
extern error TjCompressError;
extern error TjDestroyError;

extern error ImWrongSizeError;

extern error NCSDevCreateError;
extern error NCSDevOpenError;
extern error NCSGraphCreateError;
extern error NCSGraphAllocateError;
extern error NCSInferenceError;
extern error NCSGetOptError;
extern error NCSFifoReadError;
extern error NCSDestroyError;

extern error MovReadGraphFileError;
extern error MovTooFewBytesError;

#undef ERROR



#define RIFE( expr, mask, code, msg, ... )\
    if((expr)) {\
        printf("\n[%s::%d]\t"#mask" "#code"\t(errno#%d=%s)"msg".\n\n", __FILE__, __LINE__, errno, strerror(errno), ##__VA_ARGS__);\
        socket_errno = mask##code##Error;\
        return -1;\
    }

#ifdef __cplusplus
}
#endif
#endif