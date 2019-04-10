#ifndef __SOCKET_H__
#define __SOCKET_H__


#define NCS


union SockPool {
    SockPoolError,
    SockPoolTimeout,
    SockPoolRecvBusy
};

enum ErrorCode {
	StsOk=			  0,
	StsError=		 -2,
	SockBindFailed=	 -10,
    SockSendError=   -11,
    SockPoolError=   -12,
    SockPoolTimeout= -13,
    SockPoolRecvBusy=-14,
    SockReconnClosing=-15,
    PckError=        -20,
    PckSTX=          -21,
    PckSmall=        -22,
    ImTjError =      -30
};

#define ERROR(msg, ...)\
    printf("Error in %s::%d: "msg"\n", __FILE__, __LINE__, ##__VA_ARGS__);

/** expr should not be a here defined function call **/
#define test_return( expr, code, msg, ... )\
    if((expr)) {\
        printf("[Error %d] in %s::%d: "msg"\n", code, __FILE__, __LINE__, ##__VA_ARGS__);\
        return code;\
    }


#define test( expr, code, msg, ... )\
    if((expr)) {\
        printf("[Error %d] in %s::%d: "msg"\n", code, __FILE__, __LINE__, ##__VA_ARGS__);\
    }


#endif