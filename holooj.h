#ifndef __HOLOOJ_H__
#define __HOLOOJ_H__

typedef unsigned char byte;
typedef const int error;



#define REPORT_ERRNO( expr, code, msg, ... )\
    if((expr) < 0) {\
        printf("\n[%s::%d] error "#code"=%d: "msg" [%d=%s].\n\n",\
                __FILE__, __LINE__, code, errno, strerror(errno), ##__VA_ARGS__);\
        return -1;\
    }

#define REPORT( expr, code, msg, ... )\
    if((expr) < 0) {\
        printf("\n[%s::%d] error "#code"=%d: "msg".\n\n", __FILE__, __LINE__, code, ##__VA_ARGS__);\
        return -1;\
    }

#define RIFE2( expr, mask, code, msg, ... )\
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



#endif