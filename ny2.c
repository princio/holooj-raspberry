

#include "ny2.h"
#include "ncs.h"
#include "socket.h"


#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define EXPIT(X)  (1 / (1 + exp(-(X))))


int names[80];
const char categories[626] = "person\0bicycle\0car\0motorbike\0aeroplane\0bus\0train\0truck\0boat\0traffic light\0fire hydrant\0stop sign\0parking meter\0bench\0bird\0cat\0dog\0horse\0sheep\0cow\0elephant\0bear\0zebra\0giraffe\0backpack\0umbrella\0handbag\0tie\0suitcase\0frisbee\0skis\0snowboard\0sports ball\0kite\0baseball bat\0baseball glove\0skateboard\0surfboard\0tennis racket\0bottle\0wine glass\0cup\0fork\0knife\0spoon\0bowl\0banana\0apple\0sandwich\0orange\0broccoli\0carrot\0hot dog\0pizza\0donut\0cake\0chair\0sofa\0pottedplant\0bed\0diningtable\0toilet\0tvmonitor\0laptop\0mouse\0remote\0keyboard\0cell phone\0microwave\0oven\0toaster\0sink\0refrigerator\0book\0clock\0vase\0scissors\0teddy bear\0hair drier\0toothbrush";

const float yolo_biases[] = { 0.57273, 0.677385, 1.87446, 2.06253, 3.33843, 5.47434, 7.88282, 3.52778, 9.77052, 9.16828 };

float *yolo_output;
float *yolo_input;

detection *dets;

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
    int w = NY2_INPUT_W;
    int h = NY2_INPUT_H;

    int new_w=0;
    int new_h=0;
    
    if (((float)imw/w) < ((float)imh/h)) {
        new_w = imw;
        new_h = (h * imw)/w;
    } else {
        new_h = imh;
        new_w = (w * imh)/h;
    }
    for (i = 0; i < NY2_B_CELL; ++i){
        box b = dets[i].bbox;
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
        dets[i].bbox = b;
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
    int total = NY2_B_TOTAL;
    k = (total)-1;
    for(i = 0; i <= k; ++i){
        if(dets[i].objectness == 0){
            detection swap = dets[i];
            dets[i] = dets[k];
            dets[k] = swap;
            --k;
            --i;
        }
    }
    total = k+1;

    for(k = 0; k < NY2_CLASSES; ++k){
        for(i = 0; i < total; ++i){
            dets[i].sort_class = k;
        }
        qsort(dets, total, sizeof(detection), nms_comparator);
        for(i = 0; i < total; ++i){
            if(dets[i].prob[k] == 0) continue;
            box a = dets[i].bbox;
            for(j = i+1; j < total; ++j){
                box b = dets[j].bbox;
                if (box_iou(a, b) > thresh){
                    dets[j].prob[k] = 0;
                }
            }
        }
    }
}

box get_region_box(float *x, int n, int index, int i, int j)
{
    box b;
    b.x = (j + EXPIT(x[index])) / NY2_OUT_W;
    b.y = (i + EXPIT(x[index+1])) / NY2_OUT_H;
    b.w = exp(x[index + 2]) * yolo_biases[2*n]   / NY2_OUT_W;
    b.h = exp(x[index + 3]) * yolo_biases[2*n+1] / NY2_OUT_H;
    return b;
}


int get_rec_objects(rec_object *robj, float thresh, int imw, int imh)
{
    int i,j,n, nbbox;
    int wh = NY2_OUT_W*NY2_OUT_H;
    int b = 0;
    for (i = 0; i < wh; ++i){
        for(n = 0; n < NY2_B_CELL; ++n){
            int obj_index  = b + NY2_COORDS;  //entry_index(l, 0, n*yolo_w*yolo_h + i, yolo_coords);
            int box_index  = b;                //entry_index(l, 0, n*yolo_w*yolo_h + i, 0);
            int det_index = i + n*wh;           // det have the same size of output but reordered with struct
                                                // det have size 13x13x5=845, total number of bboxes.
                                                // for each one there are 2 different pointers to: bbox(5) and classes(80)
            float scale = EXPIT(yolo_output[obj_index]);

            for(j = 0; j < NY2_CLASSES; ++j){
                dets[det_index].prob[j] = 0;
            }

            dets[det_index].bbox = get_region_box(yolo_output, n, box_index, i / NY2_OUT_W, i % NY2_OUT_W);

            
            if(scale > thresh) {
                dets[det_index].objectness = scale;

                /** SOFTMAX **/
                float *pclasses = yolo_output + b + 5;
                float sum = 0;
                float largest = -FLT_MAX;
                int k;
                int n0_classes = NY2_CLASSES - 1;
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
                    dets[det_index].prob[k] = (prob > thresh) ? prob : 0;
                }
                /** SOFTMAX **/
            }
            b += 85;
        }
    }
    correct_region_boxes(imw, imh);

    do_nms_sort();

    nbbox = 0;
    for(int n = 0; n < NY2_B_CELL; n++) {
        for(int k = 0; k < NY2_CLASSES; k++) {
            if (dets[n].prob[k] > .5){
                detection d = dets[n];
                rec_object *d2 = &robj[nbbox];
                box b = d.bbox;
                const char *c = &categories[names[k]];
                int lc = strlen(c);


                // printf("\n\n%2d|%2d\t%7.6f\t", n, k, d.prob[k]);
                // printf("(%7.6f, %7.6f, %7.6f, %7.6f)\t\t%s\n", b.x, b.y, b.w, b.h, c);

                memcpy(&(d2->bbox), &(b), 16);    //BBOX
                d2->objectness = d.objectness;  //OBJ
                d2->prob = d.prob[k];           //Classness
                memcpy(&(d2->name), c, lc);    //BBOX
                d2->name[lc] = 0;
                if(++nbbox == 5) return nbbox;
            }
        }
    }
    return nbbox;
}


int ny2_init () {
    
    int b = 0;
    int i = -1;
    int j = -1;
    
    while(++i < 625) {
        if(categories[i] == '\0') {
            names[++j] = b;
            b = i + 1;
        }
    }

    yolo_input = calloc(NY2_INPUT_SIZE, sizeof(float));
    yolo_output = calloc(NY2_OUTPUT_SIZE, sizeof(float));


    dets = (detection*) calloc(NY2_B_TOTAL, sizeof(detection));
    for(int i = NY2_B_TOTAL-1; i >= 0; --i){
        dets[i].prob = (float*) calloc(NY2_CLASSES, sizeof(float));
    }

#ifdef NCS
    if(ncs_init()) return -1;

    if(ncs_load_nn("./yolo/yolov2-tiny.graph", "yolov2")) return -1;
#endif
    return 0;
}


int ny2_inference_byte(unsigned char *image, rec_object *robj, float thresh, int const imw, int const imh, int const imc) {
	int i = 0;
    int l = imw*imh*imc;
	float *y = &yolo_input[imc * imw * ((NY2_INPUT_H - imh) >> 1)];
	while(i <= l-3) {
		y[i] = image[i] / 255.; ++i;    // X[i] = imbuffer[i+2] / 255.; ++i;
		y[i] = image[i] / 255.; ++i;    // X[i] = imbuffer[i] / 255.;   ++i;
		y[i] = image[i] / 255.; ++i;    // X[i] = imbuffer[i-2] / 255.; ++i;
	}

	show_image_cv(yolo_input, "bibo", NY2_INPUT_W, NY2_INPUT_H, 1);

    return ny2_inference(robj, thresh, imw, imh);
}

/**
 * @return -1 if error; 0 if no bbox has found; x>0 if it found x bbox.
 * */ 
int ny2_inference(rec_object *robj, float thresh, const int imw, const int imh) {

#ifdef NCS
    if(ncs_inference(yolo_input, (NY2_INPUT_SIZE) << 2, yolo_output, (NY2_OUTPUT_SIZE) << 2)) return -1;
#else
    int i = -1;
    char s[20] = {0};
    FILE*f = fopen("out.txt", "r");
    while (fgets(s, 20, f) != NULL) {
       yolo_output[++i] = atof(s);
    }
    fclose(f);
#endif

    return get_rec_objects(robj, thresh, imw, imh);

    
    // c = clock() - c;
    // printf("\n-->took %f seconds to execute \n", ((double) (c)) / CLOCKS_PER_SEC); 
}


void ny2_destroy() {
#ifdef NCS
    ncs_destroy();
#endif
    for(int i = NY2_B_TOTAL-1; i >= 0; --i){
        free(dets[i].prob);
    }
    free(yolo_output);
    free(yolo_input);
    free(dets);
}