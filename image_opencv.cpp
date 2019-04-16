#ifdef OPENCV

#include "stdio.h"
#include "stdlib.h"
#include "opencv2/opencv.hpp"
#include <string.h>

#include "socket.h"

using namespace cv;

extern "C" {

byte* load_image_cv(char *filename, int *w, int *h, int *size)
{
    Mat m;
    m = imread(filename, IMREAD_ANYCOLOR);
    if(!m.data){
        fprintf(stderr, "Cannot load image \"%s\"\n", filename);
        return NULL;
    }
    *w = m.rows;
    *h = m.cols;
    *size = m.total() * m.channels();
    byte *buffer = (byte*) malloc(*size);
    memcpy(buffer, m.data, *size);
    return buffer;
}

int show_image_cv(void* im, const char* name, int w, int h, int f)
{
    Mat m = Mat(h, w, f ? CV_32FC3 : CV_8UC3, im, 0);
    imshow(name, m);
	// updateWindow("bibo");
    waitKey(1);
    return 0;
}

void make_window(char *name, int w, int h)
{
    namedWindow(name, WINDOW_NORMAL); 
    resizeWindow(name, w, h);
    if(strcmp(name, "Demo") == 0) moveWindow(name, 0, 0);
}

}

#endif
