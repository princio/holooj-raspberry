

#include "ny2.h"
#include "ncs.h"
#include "socket.h"


#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef OPENCV
#include <cv.h>
#include <highgui.h>
#endif

#define EXPIT(X)  (1 / (1 + exp(-(X))))


nnet *nn;

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

void correct_region_boxes()
{
    int i;
    int netw = nn->in_w;
    int neth = nn->in_h;

    int new_w= 0;
    int new_h= 0;

    int w = nn->im_cols;
    int h = nn->im_rows;
    if (((float)netw/w) < ((float)neth/h)) {
        new_w = netw;
        new_h = (h * netw)/w;
    } else {
        new_h = neth;
        new_w = (w * neth)/h;
    }
    for (i = 0; i < nn->nbbox_total; ++i){
        if(nn->dets[i].prob[16] > 0.5) {
            printf("\b");
        }
        box b = nn->dets[i].bbox;
        b.x =  (b.x - (netw - new_w)/2./netw) / ((float)new_w/netw); 
        b.y =  (b.y - (neth - new_h)/2./neth) / ((float)new_h/neth); 
        b.w *= (float)netw/new_w;
        b.h *= (float)neth/new_h;
        nn->dets[i].bbox = b;
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
    int total = nn->nbbox_total;
    k = (total)-1;
    for(i = 0; i <= k; ++i){
        if(nn->dets[i].objectness == 0){
            detection swap = nn->dets[i];
            nn->dets[i] = nn->dets[k];
            nn->dets[k] = swap;
            --k;
            --i;
        }
    }
    total = k+1;

    for(k = 0; k < nn->nclasses; ++k){
        for(i = 0; i < total; ++i){
            nn->dets[i].sort_class = k;
        }
        qsort(nn->dets, total, sizeof(detection), nms_comparator);
        for(i = 0; i < total; ++i){
            if(nn->dets[i].prob[k] == 0) continue;
            box a = nn->dets[i].bbox;
            for(j = i+1; j < total; ++j){
                box b = nn->dets[j].bbox;
                if (box_iou(a, b) > thresh){
                    nn->dets[j].prob[k] = 0;
                }
            }
        }
    }
}

box get_region_box(float *x, int n, int index, int i, int j)
{
    box b;
    b.x = (j + EXPIT(x[index])) / nn->out_w;
    b.y = (i + EXPIT(x[index+1])) / nn->out_h;
    b.w = exp(x[index + 2]) * nn->anchors[2*n]   / nn->out_w;
    b.h = exp(x[index + 3]) * nn->anchors[2*n+1] / nn->out_h;
    return b;
}

int get_bboxes(int nbboxes_max)
{
    int i,j,n;
    int wh = nn->out_w * nn->out_h;
    int b = 0;
    for (i = 0; i < wh; ++i){
        for(n = 0; n < nn->nbbox; ++n){
            int obj_index  = b + nn->ncoords;  //entry_index(l, 0, n*yolo_w*yolo_h + i, yolo_coords);
            int box_index  = b;                //entry_index(l, 0, n*yolo_w*yolo_h + i, 0);
            int det_index = i + n*wh;           // det have the same size of output but reordered with struct
                                                // det have size 13x13x5=845, total number of bboxes.
                                                // for each one there are 2 different pointers to: bbox(5) and classes(80)
            float scale = EXPIT(nn->output[obj_index]);

            for(j = 0; j < nn->nclasses; ++j){
                nn->dets[det_index].prob[j] = 0;
            }

            nn->dets[det_index].bbox = get_region_box(nn->output, n, box_index, i / nn->out_w, i % nn->out_w);
            
            if(scale > nn->thresh) {
                nn->dets[det_index].objectness = scale;

                /** SOFTMAX **/
                float *pclasses = nn->output + b + 5;
                float sum = 0;
                float largest = -FLT_MAX;
                int k;
                int n0_classes = nn->nclasses - 1;
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
                    nn->dets[det_index].prob[k] = (prob > nn->thresh) ? prob : 0;
                }
                /** SOFTMAX **/
            }
            b += nn->ncoords + 1 + nn->nclasses;
        }
    }

    correct_region_boxes();

    do_nms_sort();

    int nbbox = 0;
    for(int n = 0; n < nn->nbbox_total; n++) {
        for(int k = 0; k < nn->nclasses; k++) {
            if (nn->dets[n].prob[k] > .5) {
                detection d = nn->dets[n];

                bbox *_bbox = &nn->bboxes[nbbox];

                memcpy(&(_bbox->box), &(d.bbox), 16);  //BBOX
                _bbox->objectness = d.objectness;  //OBJ
                _bbox->prob = d.prob[k];           //Classness
                _bbox->cindex = k;                 //class-index

                if(++nbbox == nbboxes_max) return nbbox;
            }
        }
    }
    return nbbox;
}

/**
 * @return -1 if error; 0 if no bbox has found; x>0 if it found x bbox.
 * */ 
int ny2_inference(int nbboxes_max) {

#ifdef NOMOVIDIUS
    int i = -1;
    char s[20] = {0};
    FILE*f = fopen("out.txt", "r");
    while (fgets(s, 20, f) != NULL) {
       nn->output[++i] = atof(s);
    }
    fclose(f);
#endif

    return get_bboxes(nbboxes_max);
}
