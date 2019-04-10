#ifndef __DARKNET_REGION_LAYER_H__
#define __DARKNET_REGION_LAYER_H__

typedef struct box{
    float x, y, w, h;
} box;

typedef struct detection{
    box bbox;
    int classes;
    float *prob;
    float *mask;
    float objectness;
    int sort_class;
} detection;

unsigned int yolo_input_size;

float *yolo_output;
float *yolo_input;
int yolo_h;
int yolo_w;
int yolo_classes;
int yolo_coords;    //number of coordinates required by a bounding box
int yolo_n;         // bounding box to detect number
int yolo_outputs;
float yolo_biases[20]; 

int yolo_image_w;
int yolo_image_h;

int yolo_nboxes;

detection *yolo_dets;

void get_bboxes(float thresh);

#endif