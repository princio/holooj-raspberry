#ifndef __MOVIDIUS_H__
#define __MOVIDIUS_H__


typedef enum {
	NCSNN_YOLOv2
} NCSNNType;

typedef enum { 
    NCSDevCreateError,
    NCSDevOpenError,
	NCSGraphCreateError,
	NCSGraphAllocateError,
	NCSInferenceError,
	NCSGetOptError,
	NCSFifoReadError,
	NCSDestroyError,
    NCSReadGraphFileError,
    NCSTooFewBytesError,
    NCSParseError
} NCSerror;


typedef struct box{
    float x, y, w, h;
} box;

typedef struct bbox{
    box box;
    float objectness;
    float prob;
    int cindex;
} bbox;

typedef struct detection{
    box bbox;
    int classes;
    float *prob;
    float objectness;
    int sort_class;
} detection;

typedef struct nnet {
	char name[10];
    NCSNNType type;
    float thresh;
    int in_w;
    int in_h;
    int in_c;
    int im_cols;
    int im_rows;
    int out_w;
    int out_h;
    int out_z;
    int nbbox;
    int nbbox_total;
    int ncoords;
    int nclasses;
    int nanchors;
    int input_size_byte;
    int output_size_byte;
    char *classes_buffer;
    char **classes;
    float *anchors;
    float *output;
    float *input;
    detection *dets;
    bbox *bboxes;
} nnet;

int ncs_init(const char*, const char*, NCSNNType, nnet *nn);

int ncs_inference_byte(unsigned char *image, int nbboxes_max);
int ncs_inference(int nbboxes_max);

int ncs_destroy();

#endif //__MOVIDIUS_H__