/*
 * Gender_Age_Lbp
 *
 * Contributing Authors: Tome Vang <tome.vang@intel.com>, Neal Smith <neal.p.smith@intel.com>, Heather McCabe <heather.m.mccabe@intel.com>
 *
 *
 *
 */
#include "ncs.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <mvnc.h>

#undef RIFEM
#define RIFEM( expr, code, msg, ... )\
    if((expr)) {\
        printf("\n[%s::%d]\tNCS "#code"\t(ncscode=%d)"msg".\n\n", __FILE__, __LINE__, retCode, ##__VA_ARGS__);\
        ncs_errno = NCS##code##Error;\
        return -1;\
    }


uint32_t numNCSConnected = 0;
ncStatus_t retCode;
struct ncDeviceHandle_t* dev_handle;
struct ncGraphHandle_t* graph_handle = NULL;
struct ncFifoHandle_t* fifo_in = NULL;
struct ncFifoHandle_t* fifo_out = NULL;
NCSerror ncs_errno;


int read_graph_from_file(const char *graph_filename, unsigned int *length_read, void **graph_buf)
{
    FILE *graph_file_ptr;

    graph_file_ptr = fopen(graph_filename, "rb");

    if(graph_file_ptr == NULL) return -1;

    *length_read = 0;
    fseek(graph_file_ptr, 0, SEEK_END);
    *length_read = ftell(graph_file_ptr);
    rewind(graph_file_ptr);

    if(!(*graph_buf = malloc(*length_read))) {
        // couldn't allocate buffer
        fclose(graph_file_ptr);
        return -1;
    }

    size_t to_read = *length_read;
    size_t read_count = fread(*graph_buf, 1, to_read, graph_file_ptr);

    if(read_count != *length_read) {
        fclose(graph_file_ptr);
        free(*graph_buf);
        *graph_buf = NULL;
        return -1;
    }

    fclose(graph_file_ptr);

    return 0;
}

int ncs_init() {
    retCode = ncDeviceCreate(0, &dev_handle);
    RIFEM(retCode, DevCreate, "");

    retCode = ncDeviceOpen(dev_handle);
    RIFEM(retCode, DevOpen, "");

    return 0;
}

int ncs_load_nn(const char *graph_path, const char *graph_name){
    
    unsigned int graph_len = 0;
    void *graph_buf;

    retCode = read_graph_from_file(graph_path, &graph_len, &graph_buf);
    RIFEM(retCode, ReadGraphFile, "");


    retCode = ncGraphCreate(graph_name, &graph_handle);
    RIFEM(retCode, GraphCreate, "");


    retCode = ncGraphAllocateWithFifosEx(dev_handle, graph_handle, graph_buf, graph_len,
                                        &fifo_in, NC_FIFO_HOST_WO, 2, NC_FIFO_FP32,
                                        &fifo_out, NC_FIFO_HOST_RO, 2, NC_FIFO_FP32);
    RIFEM(retCode, GraphAllocate, "");

    free(graph_buf);

    return 0;
}

int ncs_inference(float *in, unsigned int in_size_bytes, float *out, int out_size_bytes) {

    unsigned int length_bytes;
    unsigned int returned_opt_size;
    retCode = ncGraphQueueInferenceWithFifoElem(graph_handle, fifo_in, fifo_out, in, &in_size_bytes, 0);
    RIFEM(retCode, Inference, "");

    returned_opt_size=4;
    retCode = ncFifoGetOption(fifo_out, NC_RO_FIFO_ELEMENT_DATA_SIZE, &length_bytes, &returned_opt_size);
    RIFEM(retCode, GetOpt, "");

    RIFEM(length_bytes != out_size_bytes, TooFewBytes, "(%d != %d)", length_bytes, out_size_bytes);

    retCode = ncFifoReadElem(fifo_out, out, &length_bytes, NULL);
    RIFEM(retCode, FifoRead, "");

    return 0;
}

int ncs_destroy() {
    RIFEM(retCode = ncFifoDestroy(&fifo_in), Destroy, "");
    RIFEM(retCode = ncFifoDestroy(&fifo_out), Destroy, "");
    RIFEM(retCode = ncGraphDestroy(&graph_handle), Destroy, "");
    RIFEM(retCode = ncDeviceClose(dev_handle), Destroy, "");
    RIFEM(retCode = ncDeviceDestroy(&dev_handle), Destroy, "");
    return 0;
}

#undef RIFEM