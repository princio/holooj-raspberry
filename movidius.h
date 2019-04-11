#ifndef __MOVIDIUS_H__
#define __MOVIDIUS_H__

#define NCS2

#define RIFEM( expr, mask, code, msg, ... )\
    if((expr)) {\
        printf("\n[%s::%d]\t"#mask" "#code"\t(ncscode=%d)"msg".\n\n", __FILE__, __LINE__, retCode, ##__VA_ARGS__);\
        movidius_errno = mask##code##Error;\
        return -1;\
    }


int movidius_errno;
int nc_error_code;

int mov_init();

int mov_inference(float *output, int *output_size, float thresh);

int mov_destroy();


#endif //__MOVIDIUS_H__