#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "yolov2.h"

#define EXPIT(X)  (1 / (1 + exp(-(X))))

detection *dets;

const char categories[626] = "person\0bicycle\0car\0motorbike\0aeroplane\0bus\0train\0truck\0boat\0traffic light\0fire hydrant\0stop sign\0parking meter\0bench\0bird\0cat\0dog\0horse\0sheep\0cow\0elephant\0bear\0zebra\0giraffe\0backpack\0umbrella\0handbag\0tie\0suitcase\0frisbee\0skis\0snowboard\0sports ball\0kite\0baseball bat\0baseball glove\0skateboard\0surfboard\0tennis racket\0bottle\0wine glass\0cup\0fork\0knife\0spoon\0bowl\0banana\0apple\0sandwich\0orange\0broccoli\0carrot\0hot dog\0pizza\0donut\0cake\0chair\0sofa\0pottedplant\0bed\0diningtable\0toilet\0tvmonitor\0laptop\0mouse\0remote\0keyboard\0cell phone\0microwave\0oven\0toaster\0sink\0refrigerator\0book\0clock\0vase\0scissors\0teddy bear\0hair drier\0toothbrush";
int names[80];

float *yolo_output;
float *yolo_input;
int yolo_h;
int yolo_w;
int yolo_classes;
int yolo_coords;    //number of coordinates required by a bounding box
int yolo_n;         // bounding box to detect number
int yolo_outputs;
float yolo_biases[20]; 

int yolo_input_w = 416;
int yolo_input_h = 416;
unsigned int yolo_input_size;

int yolo_nboxes;


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
    yolo_input_w = 416;
    yolo_input_h = 416;
    yolo_input_size = yolo_input_w*yolo_input_h*3;
    yolo_input = calloc(yolo_input_size, sizeof(float));

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

    /** LAYER **/


    /** DETECTIONs **/
    yolo_dets = (detection*) calloc(yolo_nboxes, sizeof(detection));
    for(int i = yolo_nboxes-1; i >= 0; --i){
        yolo_dets[i].prob = (float*) calloc(yolo_classes, sizeof(float));
    }
    /** DETECTIONs **/
}

float overlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1/2;
    float l2 = x2 - w2/2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1/2;
    float r2 = x2 + w2/2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

float box_intersection(box a, box b)
{
    float w = overlap(a.x, a.w, b.x, b.w);
    float h = overlap(a.y, a.h, b.y, b.h);
    if(w < 0 || h < 0) return 0;
    float area = w*h;
    return area;
}

float box_union(box a, box b)
{
    float i = box_intersection(a, b);
    float u = a.w*a.h + b.w*b.h - i;
    return u;
}

float box_iou(box a, box b)
{
    return box_intersection(a, b)/box_union(a, b);
}

void correct_region_boxes(int imw, int imh)
{
    int i;
    int w = yolo_input_w;
    int h = yolo_input_h;

    int new_w=0;
    int new_h=0;
    
    if (((float)imw/w) < ((float)imh/h)) {
        new_w = imw;
        new_h = (h * imw)/w;
    } else {
        new_h = imh;
        new_w = (w * imh)/h;
    }
    for (i = 0; i < yolo_n; ++i){
        box b = yolo_dets[i].bbox;
        b.x =  (b.x - (imw - new_w)/2./imw) / ((float)new_w/imw); 
        b.y =  (b.y - (imh - new_h)/2./imh) / ((float)new_h/imh); 
        b.w *= (float)imw/new_w;
        b.h *= (float)imh/new_h;
        // if(!relative){ //set to 1 in darknet:detector.c
        //     b.x *= w;
        //     b.w *= w;
        //     b.y *= h;
        //     b.h *= h;
        // }
        yolo_dets[i].bbox = b;
    }
}

int nms_comparator(const void *pa, const void *pb)
{
    detection a = *(detection *)pa;
    detection b = *(detection *)pb;
    float diff = 0;
    if(b.sort_class >= 0){
        diff = a.prob[b.sort_class] - b.prob[b.sort_class];
    } else {
        diff = a.objectness - b.objectness;
    }
    if(diff < 0) return 1;
    else if(diff > 0) return -1;
    return 0;
}

/**
 * @brief NMS sort
 * @param total    The total number of bounding boxes
 * @param classes  number of classes
 */
void do_nms_sort()
{
    float thresh = .45; // like in darknet:detector#575
    int i, j, k;
    int total = yolo_w*yolo_h*yolo_n;
    k = (total)-1;
    for(i = 0; i <= k; ++i){
        if(yolo_dets[i].objectness == 0){
            detection swap = yolo_dets[i];
            yolo_dets[i] = yolo_dets[k];
            yolo_dets[k] = swap;
            --k;
            --i;
        }
    }
    total = k+1;

    for(k = 0; k < yolo_classes; ++k){
        for(i = 0; i < total; ++i){
            yolo_dets[i].sort_class = k;
        }
        qsort(yolo_dets, total, sizeof(detection), nms_comparator);
        for(i = 0; i < total; ++i){
            if(yolo_dets[i].prob[k] == 0) continue;
            box a = yolo_dets[i].bbox;
            for(j = i+1; j < total; ++j){
                box b = yolo_dets[j].bbox;
                if (box_iou(a, b) > thresh){
                    yolo_dets[j].prob[k] = 0;
                }
            }
        }
    }
}

box get_region_box(float *x, int n, int index, int i, int j)
{
    box b;
    b.x = (j + EXPIT(x[index])) / yolo_w;
    b.y = (i + EXPIT(x[index+1])) / yolo_h;
    b.w = exp(x[index + 2]) * yolo_biases[2*n]   / yolo_w;
    b.h = exp(x[index + 3]) * yolo_biases[2*n+1] / yolo_h;
    return b;
}


void get_bboxes(float thresh, int imw, int imh)
{
    int i,j,n;
    int wh = yolo_w*yolo_h;
    int b = 0;
    for (i = 0; i < wh; ++i){
        for(n = 0; n < yolo_n; ++n){
            int obj_index  = b + yolo_coords;  //entry_index(l, 0, n*yolo_w*yolo_h + i, yolo_coords);
            int box_index  = b;                //entry_index(l, 0, n*yolo_w*yolo_h + i, 0);
            int det_index = i + n*wh;           // det have the same size of output but reordered with struct
                                                // det have size 13x13x5=845, total number of bboxes.
                                                // for each one there are 2 different pointers to: bbox(5) and classes(80)
            float scale = EXPIT(yolo_output[obj_index]);

            for(j = 0; j < yolo_classes; ++j){
                yolo_dets[det_index].prob[j] = 0;
            }

            yolo_dets[det_index].bbox = get_region_box(yolo_output, n, box_index, i / yolo_w, i % yolo_w);

            
            if(1 || scale > thresh) {
                yolo_dets[det_index].objectness = scale;

                /** SOFTMAX **/
                float *pclasses = yolo_output + b + 5;
                float sum = 0;
                float largest = -FLT_MAX;
                int k;
                int n0_classes = yolo_classes - 1;
                for(k = n0_classes; k >= 0; --k){
                    if(pclasses[k] > largest) largest = pclasses[k];
                }
                for(k = n0_classes; k >= 0; --k){
                    float e = exp(pclasses[k] - largest);
                    sum += e;
                    pclasses[k] = e;
                }
                for(k = n0_classes; k >= 0; --k){
                    float prob = scale * pclasses[k] / sum;
                    yolo_dets[det_index].prob[k] = (prob > thresh) ? prob : 0;
                }
                /** SOFTMAX **/
            }
            b += 85;
        }
    }
    correct_region_boxes(imw, imh);

    do_nms_sort();
}