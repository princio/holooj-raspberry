#ifndef __MOVIDIUS_H__
#define __MOVIDIUS_H__

#define NCS2

#define ASS(A,B) if((A)) return error_code = (B)

typedef unsigned char byte;

typedef enum {
    MOV_OK = 0,
    MOV_GRAPH_OPEN = -5,
    MOV_GRAPH_ALLC_BUFFER = -6,
    MOV_GRAPH_READ = -7,
    MOV_INIT_NOTFOUND = -8,
    MOV_INIT_NOTOPEN = -9
} movStatus_t;


int error_code;
int nc_error_code;

int mov_init();

int mov_inference(float *output, int *output_size, float thresh);

void mov_destroy();


#endif //__MOVIDIUS_H__