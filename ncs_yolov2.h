#ifndef __YOLOv2_H__
#define __YOLOv2_H__

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


detection *yolo_dets;

void get_bboxes(float thresh, int imw, int imh);

#endif