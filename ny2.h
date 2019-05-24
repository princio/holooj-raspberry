#ifndef __YOLOv2_H__
#define __YOLOv2_H__

#include "ncs.h"



int  ny2_init(const char *graph, const char *meta, nnet *nn );
int  ny2_inference_byte(unsigned char *image, int nbboxes_max);
int  ny2_inference(int nbboxes_max);


#endif