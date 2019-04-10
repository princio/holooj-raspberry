#ifndef __SOCKET_H__
#define __SOCKET_H__


#define NCS

#define ERROR(M1, V1, M2, V2) error M1##M2##Error = V1 | 1 << (8+V2);


typedef unsigned char byte;
typedef const int error;


error Gen_Error = INT32_MAX;
error SO=    1;
error Poll=  2;
error Pck=   4;
error Tj=    8;
error Im=    16;
error Im=    16;

ERROR(   SO,     1,     Creation,     1);
ERROR(   SO,     1,         Bind,     2);
ERROR(   SO,     1,    Listening,     3);
ERROR(   SO,     1,       Accept,     4);
ERROR(   SO,     1,         Send,     5);
ERROR(   SO,     1,         Recv,     6);
ERROR(   SO,     1,         Addr,     7);

ERROR( Poll,     2,      Generic,     1);
ERROR( Poll,     2,      Timeout,     2);
ERROR( Poll,     2,     RecvBusy,     3);

ERROR(  Pck,     3,        Error,     1);
ERROR(  Pck,     3,          STX,     2);
ERROR(  Pck,     3,     TooSmall,     3);

ERROR(   Tj,     4,      Generic,     1);
ERROR(   Tj,     4,       Header,     2);
ERROR(   Tj,     4,   Decompress,     3);
ERROR(   Tj,     4,     Compress,     4);
ERROR(   Tj,     4,      Destroy,     5);

ERROR(   Im,     5,    WrongSize,     1);

#undef ERROR


#define ERROR(msg, ...)\
    printf("Error in %s::%d: "msg"\n", __FILE__, __LINE__, ##__VA_ARGS__);

/** expr should not be a here defined function call **/
#define RIFE( expr, mask, code, msg, ... )\
    if((expr)) {\
        printf("[Error %s::%d] "#mask"-"#code" [errno=%d] "#msg".\n", mask##code##Error, __FILE__, __LINE__, errno, ##__VA_ARGS__);\
        socket_errno = mask##code##Error;\
        return -1;\
    }


#define test( expr, code, msg, ... )\
    if((expr)) {\
        printf("[Error %d] in %s::%d: "msg"\n", code, __FILE__, __LINE__, ##__VA_ARGS__);\
    }


#endif