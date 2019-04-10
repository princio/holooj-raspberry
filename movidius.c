/*
 * Gender_Age_Lbp
 *
 * Contributing Authors: Tome Vang <tome.vang@intel.com>, Neal Smith <neal.p.smith@intel.com>, Heather McCabe <heather.m.mccabe@intel.com>
 *
 *
 *
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <mvnc.h>

#ifdef NCS
#include <mvnc.h>
#endif

#include "movidius.h"
#include "yolo.h"
#include "socket.h"

// network image resolution
#define MOV_IM_W 416
#define MOV_IM_H 416
#define MOV_IM_C 3
#define MOV_IM_SIZE (MOV_IM_W*MOV_IM_H*MOV_IM_C)

// Location of age and gender networks
#define YOLO_GRAPH_DIR "yolo/"
#define YOLO_CAT_STAT_DIRECTORY "yolo/"

#define MAX_NCS_CONNECTED 1

typedef unsigned short half_float;

const unsigned int MAX_PATH = 256;

/** NCS **/
#ifdef NCS
uint32_t numNCSConnected = 0;
ncStatus_t retCode;
struct ncDeviceHandle_t* ncs_dev_handle;
struct ncGraphHandle_t* ncs_graph_handle = NULL;
struct ncFifoHandle_t* ncs_fifo_in = NULL;
struct ncFifoHandle_t* ncs_fifo_out = NULL;
#endif

/** DARKNET */
detection *dets;

const char categories[626] = "person\0bicycle\0car\0motorbike\0aeroplane\0bus\0train\0truck\0boat\0traffic light\0fire hydrant\0stop sign\0parking meter\0bench\0bird\0cat\0dog\0horse\0sheep\0cow\0elephant\0bear\0zebra\0giraffe\0backpack\0umbrella\0handbag\0tie\0suitcase\0frisbee\0skis\0snowboard\0sports ball\0kite\0baseball bat\0baseball glove\0skateboard\0surfboard\0tennis racket\0bottle\0wine glass\0cup\0fork\0knife\0spoon\0bowl\0banana\0apple\0sandwich\0orange\0broccoli\0carrot\0hot dog\0pizza\0donut\0cake\0chair\0sofa\0pottedplant\0bed\0diningtable\0toilet\0tvmonitor\0laptop\0mouse\0remote\0keyboard\0cell phone\0microwave\0oven\0toaster\0sink\0refrigerator\0book\0clock\0vase\0scissors\0teddy bear\0hair drier\0toothbrush";

// char *names[80];
int names[80];


#ifdef NCS
/**
 * @brief read_graph_from_file
 * @param graph_filename [IN} is the full path (or relative) to the graph file to read.
 * @param length [OUT] upon successful return will contain the number of bytes read
 *        which will correspond to the number of bytes in the buffer (graph_buf) allocated
 *        within this function.
 * @param graph_buf [OUT] should be set to the address of a void pointer prior to calling
 *        this function.  upon successful return the void* pointed to will point to a
 *        memory buffer which contains the graph file that was read from disk.  This buffer
 *        must be freed when the caller is done with it via the free() system call.
 * @return true if worked and program should continue or false there was an error.
 */
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
    if(ncDeviceCreate(0, &ncs_dev_handle) != NC_OK) return -1;
    if(ncDeviceOpen(ncs_dev_handle) != NC_OK) return -1;
    return 0;
}

int ncs_load_nn(){
    
    char yolo_graph_filename[MAX_PATH];
    unsigned int yolo_graph_len = 0;
    void *yolo_graph_buf;


    strncpy(yolo_graph_filename, YOLO_GRAPH_DIR, MAX_PATH);
    strncat(yolo_graph_filename, "yolov2-tiny.graph", MAX_PATH);


    if ((retCode = read_graph_from_file(yolo_graph_filename, &yolo_graph_len, &yolo_graph_buf))) return retCode;


    if ((retCode = ncGraphCreate("yoloGraph", &ncs_graph_handle))) return retCode;


    retCode = ncGraphAllocateWithFifosEx(ncs_dev_handle, ncs_graph_handle, yolo_graph_buf, yolo_graph_len,
                                        &ncs_fifo_in, NC_FIFO_HOST_WO, 2, NC_FIFO_FP32,
                                        &ncs_fifo_out, NC_FIFO_HOST_RO, 2, NC_FIFO_FP32);
    
    return 0;
}

int ncs_inference() {

    unsigned int length_bytes;
    unsigned int returned_opt_size;
    unsigned int yolo_input_size_bytes = yolo_input_size << 2;
    retCode = ncGraphQueueInferenceWithFifoElem(ncs_graph_handle, ncs_fifo_in, ncs_fifo_out, yolo_input, &yolo_input_size_bytes, 0);

    returned_opt_size=4;
    retCode = ncFifoGetOption(ncs_fifo_out, NC_RO_FIFO_ELEMENT_DATA_SIZE, &length_bytes, &returned_opt_size);


    if(length_bytes != yolo_outputs*4) {
        printf("Error in yolo inference: too few bytes for output.");
        return -1;
    }

    retCode = ncFifoReadElem(ncs_fifo_out, yolo_output, &length_bytes, NULL);

    // FILE*f = fopen("out.txt", "w");
    // for(int i = 0; i < yolo_outputs; i++) {
    //     fprintf(f, "% 6.4f\n", yolo_output[i]);
    // }
    // fclose(f);

    return retCode;
}
#endif



void darknet_init () {
    
    int b = 0;
    int i = -1;
    int j = -1;
    
    while(++i < 625) {
        if(categories[i] == '\0') {
            names[++j] = b;
            b = i + 1;
        }
    }

    /** LAYER **/
    yolo_w = 13;
    yolo_h = 13;
    yolo_n = 5;
    yolo_coords = 4;
    yolo_classes = 80;
    yolo_biases[0] = 0.57273;
    yolo_biases[1] = 0.677385;
    yolo_biases[2] = 1.87446;
    yolo_biases[3] = 2.06253;
    yolo_biases[4] = 3.33843;
    yolo_biases[5] = 5.47434;
    yolo_biases[6] = 7.88282;
    yolo_biases[7] = 3.52778;
    yolo_biases[8] = 9.77052;
    yolo_biases[9] = 9.16828;
    yolo_outputs = yolo_h*yolo_w*yolo_n*(yolo_classes + yolo_coords + 1);
    yolo_nboxes = yolo_w*yolo_h*yolo_n;

    yolo_output = calloc(yolo_outputs, sizeof(float));
    yolo_input_size = MOV_IM_SIZE;
    yolo_input = calloc(MOV_IM_SIZE, sizeof(float));

    /** LAYER **/


    /** DETECTIONs **/
    yolo_dets = (detection*) calloc(yolo_nboxes, sizeof(detection));
    for(int i = yolo_nboxes-1; i >= 0; --i){
        yolo_dets[i].prob = (float*) calloc(yolo_classes, sizeof(float));
    }
    /** DETECTIONs **/
}


int mov_inference(float *output, int *output_size, float thresh) {

#ifdef NCS
    ncs_inference();
#else
    int i = -1;
    char s[20] = {0};
    FILE*f = fopen("out.txt", "r");
    while (fgets(s, 20, f) != NULL) {
       yolo_output[++i] = atof(s);
    }
    fclose(f);
#endif


    float c = clock();
    get_bboxes(thresh);
    
    for(int n = 0; n < yolo_nboxes; n++) {
        for(int k = 0; k < yolo_classes; k++) {
            if (yolo_dets[n].prob[k] > .5){
                detection d = yolo_dets[n];
                box b = d.bbox;


                printf("\n\n%2d|%2d\t%7.6f\t\t", n, k, d.prob[k]);
                printf("(%7.6f, %7.6f, %7.6f, %7.6f)\n", b.x, b.y, b.w, b.h);

                output[4] = d.objectness;
                output[5] = d.prob[k];
                memcpy(output, &(b), sizeof(box));

                const char *c = &categories[names[k]];
                int lc = strlen(c);
                memcpy(&output[6], &lc, 4);
                memcpy(&output[7], c, lc);

                // printf("%2d|%2d|%2d\t%2d\t%7.6f\t\t", i, j, n, k, yolo_dets[id].prob[k]);
                // box b = yolo_dets[id].bbox;
                // printf("(%7.6f, %7.6f, %7.6f, %7.6f)\n", b.x, b.y, b.w, b.h);

                *output_size = 5*4+lc;
                break;
            }
        }
    }

    c = clock() - c;

    printf("\n-->took %f seconds to execute \n", ((double) (c)) / CLOCKS_PER_SEC); 
    return 0;
}


int mov_init () {
    // setvbuf(stdout, NULL, _IONBF, 0);


#ifdef NCS
    ncs_init();
    ncs_load_nn();
#endif

    darknet_init ();

    return 0;
}

void mov_destroy() {
    for(int i = yolo_nboxes-1; i >= 0; --i){
        free(yolo_dets[i].prob);
    }
    free(yolo_output);
    free(yolo_input);
    free(yolo_dets);
#ifdef NCS
    ncFifoDestroy(&ncs_fifo_in);
    ncFifoDestroy(&ncs_fifo_out);
    ncDeviceClose(ncs_dev_handle);
    ncDeviceDestroy(&ncs_dev_handle);
#endif
}

