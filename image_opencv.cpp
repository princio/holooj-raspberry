#ifdef OPENCV

#include "stdio.h"
#include "stdlib.h"
#include "opencv2/opencv.hpp"

#include "socket.h"

using namespace cv;

extern "C" {

Mat image_to_mat(void *im, int w, int h)
{
    return Mat(h, w, CV_8UC3, im, 0);;
}

int show_image_cv(void* im, const char* name, int w, int h)
{
    Mat m = image_to_mat(im, w, h);
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
