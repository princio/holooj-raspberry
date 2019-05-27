/*
 * Gender_Age_Lbp
 *
 * Contributing Authors: Tome Vang <tome.vang@intel.com>, Neal Smith <neal.p.smith@intel.com>, Heather McCabe <heather.m.mccabe@intel.com>
 *
 *
 *
 */
#include "holooj.h"
#include "ncs.h"
#include "ny2.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <mvnc.h>



uint32_t numNCSConnected = 0;
ncStatus_t retCode;
struct ncDeviceHandle_t* dev_handle;
struct ncGraphHandle_t* graph_handle = NULL;
struct ncFifoHandle_t* fifo_in = NULL;
struct ncFifoHandle_t* fifo_out = NULL;
NCSerror ncs_errno;

nnet *nn;


int parse_meta_file(const char *meta) {

    char *buf;
    FILE *f = fopen(meta, "r");

    REPORT(f == NULL, NCSParseError, "Parsing error");

    unsigned int length_read;
    fseek(f, 0, SEEK_END);
    length_read = ftell(f);
    rewind(f);

    if(!(buf = malloc(length_read))) {
        // couldn't allocate buffer
        fclose(f);
        return -1;
    }

    size_t to_read = length_read;
    size_t read_count = fread(buf, 1, to_read, f);

    if(read_count != length_read) {
        fclose(f);
        free(buf);
        buf = NULL;
        return -1;
    }

    fclose(f);
    
    char *labels;
    char *anchors_text;
    char *in_size;
    char *out_size;
    char *pch = strtok( buf, "\"\"" );
    while (pch != NULL)
    {
        if(!strcmp(pch, "labels")) {
            pch = strtok (NULL, "[");
            labels = strtok (NULL, "]");
        } else
        if(!strcmp(pch, "anchors")) {
            pch = strtok (NULL, "[");
            anchors_text = strtok (NULL, "]");
        } else
        if(!strcmp(pch, "classes")) {
            pch = strtok (NULL, ",\"}");
            sscanf(pch, "%*[^0123456789]%d", &nn->nclasses);
        } else
        if(!strcmp(pch, "inp_size")) {
            pch = strtok (NULL, "[");
            in_size = strtok (NULL, "]");
            sscanf(in_size, "%d%*[^0123456789]%d%*[^0123456789]%d", &nn->in_w, &nn->in_h, &nn->in_c);
        } else
        if(!strcmp(pch, "out_size")) {
            pch = strtok (NULL, "[");
            out_size = strtok (NULL, "]");
            sscanf(out_size, "%d%*[^0123456789]%d%*[^0123456789]%d", &nn->out_w, &nn->out_h, &nn->out_z);
        } else
        if(!strcmp(pch, "num")) {
            pch = strtok (NULL, ",}\"");
            sscanf(pch, "%*[^0123456789]%d", &nn->nbbox);
        } else
        if(!strcmp(pch, "coords")) {
            pch = strtok (NULL, ",}\"");
            sscanf(pch, "%*[^0123456789]%d", &nn->ncoords);
        }
        pch = strtok (NULL, "\"");
    }

    nn->classes_buffer = calloc(strlen(labels) + 80, 1);
    nn->classes = calloc(nn->nclasses, sizeof(char*));
    pch = strtok(labels, "\"");
    int i = 0;
    int l = 0;
    while (pch != NULL)
    {
        if(strstr(pch, ",") == NULL) {
            strcpy(nn->classes_buffer + l, pch);
            nn->classes[i++] = nn->classes_buffer + l;
            l += strlen(pch);
            nn->classes_buffer[l] = '\0';
            ++l;
        }
        pch = strtok(NULL, "\"");
    }

    nn->nanchors = 1;
    pch = strpbrk (anchors_text, ",");
    while (pch != NULL)
    {
        ++nn->nanchors;
        pch = strpbrk (pch+1, ",");
    }

    nn->anchors = calloc(nn->nanchors, sizeof(float));
    pch = strtok(anchors_text, ",]");
    i = -1;
    while (pch != NULL)
    {
        sscanf(pch, "%f", &nn->anchors[++i]);
        pch = strtok(NULL, ",]");
    }
    printf("\n\n");


    printf("\nYOLOv2:\n");

    printf("\n\tclasses[%d]:\n\t\t", nn->nclasses);
    for(i=0; i < nn->nclasses; i++) printf("%s%s", nn->classes[i], (i > 0 && i % 10 == 0) ? "\n\t\t" : ", ");

    printf("\n\n\tanchors[%d]:\n\t\t", nn->nanchors);
    for(i=0; i < nn->nanchors; i++) printf("%f%s", nn->anchors[i], (i > 0 && i %  3 == 0) ? "\n\t\t" : ", ");

    printf("\n\n\t input: [ %5d, %5d, %5d ]\n\toutput: [ %5d, %5d, %5d ]\n\t nbbox: %d\n\t ncoords: %d\n",
            nn->in_w, nn->in_h, nn->in_c, nn->out_w, nn->out_h, nn->out_z, nn->nbbox, nn->ncoords);

    nn->nbbox_total = nn->out_w * nn->out_h * nn->nbbox;
    nn->input_size_byte = 4 * nn->in_w * nn->in_h * nn->in_c;
    nn->output_size_byte = 4 * nn->out_w * nn->out_h * nn->nbbox * (nn->ncoords + 1 + nn->nclasses);
    printf("\n\t input size: %d\n\toutput_size: %d\n\nnbbox total: %d\n\n",
            nn->input_size_byte, nn->output_size_byte, nn->nbbox_total);

    free(buf);

    printf("NN:\n");
    printf("\t%20s:\t%s\n", "name", nn->name);
    printf("\t%20s:\t%f\n", "thresh", nn->thresh);
    printf("\t%20s:\t[ %d, %d, %d]\n", "input", nn->in_w, nn->in_h, nn->in_c);
    printf("\t%20s:\t[ %d, %d ]\n", "image", nn->im_cols, nn->im_rows);
    printf("\t%20s:\t[ %d, %d, %d ]\n", "output", nn->out_w, nn->out_h, nn->out_z);
    printf("\t%20s:\t%d\n", "nbbox", nn->nbbox);
    printf("\t%20s:\t%d\n", "nbbox_total", nn->nbbox_total);
    printf("\t%20s:\t%d\n", "ncoords", nn->ncoords);
    printf("\t%20s:\t%d\n", "nclasses", nn->nclasses);
    printf("\t%20s:\t%d\n", "nanchors", nn->nanchors);
    printf("\t%20s:\t%d\n", "input_size_byte", nn->input_size_byte);
    printf("\t%20s:\t%d\n", "output_size_byte", nn->output_size_byte);

    return 0;
}


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


int ncs_load_nn(const char *graph_path){
    
    unsigned int graph_len = 0;
    void *graph_buf;

    retCode = read_graph_from_file(graph_path, &graph_len, &graph_buf);
    REPORT(retCode, NCSReadGraphFileError, "");


    retCode = ncGraphCreate(nn->name, &graph_handle);
    REPORT(retCode, NCSGraphCreateError, "");


    retCode = ncGraphAllocateWithFifosEx(dev_handle, graph_handle, graph_buf, graph_len,
                                        &fifo_in, NC_FIFO_HOST_WO, 2, NC_FIFO_FP32,
                                        &fifo_out, NC_FIFO_HOST_RO, 2, NC_FIFO_FP32);
    REPORT(retCode, NCSGraphAllocateError, "");

    free(graph_buf);

    return 0;
}

int ncs_init(const char *graph, const char *meta, NCSNNType nntype, nnet *_nn) {

    nn = calloc(1, sizeof(nnet));
    _nn = nn;

    if(parse_meta_file(meta))
        return -1;

    switch(nntype) {
        case NCSNN_YOLOv2:
            nn->type = NCSNN_YOLOv2;
            strcpy(nn->name, "yolov2");
        break;
    }


    nn->input = calloc(nn->input_size_byte, 1);
    nn->output = calloc(nn->output_size_byte, 1);

    retCode = ncDeviceCreate(0, &dev_handle);
    REPORT(retCode, NCSDevCreateError, "");

    retCode = ncDeviceOpen(dev_handle);
    REPORT(retCode, NCSDevOpenError, "");


    nn->dets = (detection*) calloc(nn->nbbox_total, sizeof(detection));

    for(int i = nn->nbbox_total-1; i >= 0; --i) {
        nn->dets[i].prob = (float*) calloc(nn->nclasses, sizeof(float));
    }


    if(ncs_load_nn(graph)) return -1;

    return 0;
}


int ncs_inference_byte(unsigned char *image, int nbboxes_max) {
	int i = 0;
    int letterbox = nn->in_c * nn->in_w * ((nn->in_h - nn->im_rows) >> 1);
    int l = nn->im_rows * nn->im_cols * nn->in_c;
	float *y = &nn->input[letterbox];
	while(i <= l-3) {
		y[i] = image[i + 2] / 255.; ++i;    // X[i] = imbuffer[i+2] / 255.; ++i;
		y[i] = image[i] / 255.;     ++i;    // X[i] = imbuffer[i] / 255.;   ++i;
		y[i] = image[i - 2] / 255.; ++i;    // X[i] = imbuffer[i-2] / 255.; ++i;
	}

#ifdef OPENCV
    IplImage *iplim = cvCreateImage(cvSize(nn->in_w, nn->in_h), IPL_DEPTH_32F, 3);
    memcpy(iplim->imageData, nn->input, nn->input_size_byte*4);
    cvShowImage("bibo2", iplim);
    cvUpdateWindow("bibo2");
    cvReleaseImage(&iplim);
    // cvWaitKey(0);
#endif

    int nbbox = ncs_inference(nbboxes_max);
    
    if(nbbox < 0) return -1;

    return nbbox;
}


int ncs_inference(int nbboxes_max) {

    unsigned int length_bytes;
    unsigned int returned_opt_size;
    unsigned int in_size_bytes = nn->input_size_byte;
    unsigned int out_size_bytes = nn->output_size_byte;

    retCode = ncGraphQueueInferenceWithFifoElem(graph_handle, fifo_in, fifo_out, nn->input, &in_size_bytes, 0);
    REPORT(retCode, NCSInferenceError, "recode=%d", retCode);

    returned_opt_size = 4;
    retCode = ncFifoGetOption(fifo_out, NC_RO_FIFO_ELEMENT_DATA_SIZE, &length_bytes, &returned_opt_size);
    REPORT(retCode, NCSGetOptError, "recode=%d", retCode);

    REPORT(length_bytes != out_size_bytes, NCSTooFewBytesError, "(%d != %d)", length_bytes, out_size_bytes);

    retCode = ncFifoReadElem(fifo_out, nn->output, &length_bytes, NULL);
    REPORT(retCode, NCSFifoReadError, "recode=%d", retCode);

    int nbbox = -1;
    switch(nn->type) {
        case NCSNN_YOLOv2:
            nbbox = ny2_inference(nbboxes_max);
            if(nbbox < 0) return -1;
        break;
    }
    return nbbox;
}

int ncs_destroy() {
    int er = NCSDestroyError;
    REPORT(retCode = ncFifoDestroy(&fifo_in), er, "");
    REPORT(retCode = ncFifoDestroy(&fifo_out), er, "");
    REPORT(retCode = ncGraphDestroy(&graph_handle), er, "");
    REPORT(retCode = ncDeviceClose(dev_handle), er, "");
    REPORT(retCode = ncDeviceDestroy(&dev_handle), er, "");

    for(int i = nn->nbbox_total-1; i >= 0; --i){
        free(nn->dets[i].prob);
    }
    free(nn->anchors);
    free(nn->classes);
    free(nn->classes_buffer);
    free(nn->output);
    free(nn->input);
    free(nn->dets);
    free(nn);

    return 0;
}

#undef REPORT