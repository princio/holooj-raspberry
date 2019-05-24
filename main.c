



#include <stdio.h>
#include <string.h>
#include <turbojpeg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include<time.h>

#include "holooj.h"
#include "socket.h"
#include "ncs.h"

typedef enum {
        PckGeneric,
        PckSTX,
        PckTooSmall
} PckError;

typedef enum {
        TjGeneric,
        TjHeader,
        TjDecompress,
        TjCompress,
        TjDestroy
} TjError;


typedef struct RecvPacket
{
    int stx;
    int index;
    int l;
    byte image[];
} RecvPacket;

typedef struct SendPacket
{
    int stx;
    int index;
    int n;
    bbox bboxes[5];
} SendPacket;


typedef struct Config {
	int STX;
	int rows;
	int cols;
	int isBMP;
} Config;


int elaborate_packet(RecvPacket *rbuffer, int rl, SendPacket *sbuffer);
int elaborate_image(byte *imbuffer, int iml, int index);




const int STX = 767590;
const int OH_SIZE = 12;
int buffer_size;
int image_size;
float thresh = 0.5;
Config config;
nnet *nn;




int decode_jpeg(byte *imj, int imjl, byte *im) {
    tjhandle tjh;
    int w, h, ss, cl, ret;
    
    tjh = tjInitDecompress();
		
    if(tjDecompressHeader3(tjh, imj, imjl, &w, &h, &ss, &cl)) {
        REPORT(1, TjHeader, "(%s)", tjGetErrorStr());
    }
    switch(cl) {
        case TJCS_CMYK: printf("CMYK"); break;
        case TJCS_GRAY: printf("GRAY"); break;
        case TJCS_RGB: printf("RGB"); break;
        case TJCS_YCbCr: printf("YCbCr"); break;
        case TJCS_YCCK: printf("YCCK"); break;
    }
	REPORT(cl != TJCS_RGB, ImUnsopportedColorSpace, "");
	ret = tjDecompress2(tjh, imj, imjl, im, 0, 0, 0, TJPF_RGB, 0);
	REPORT(ret, TjDecompress, "(%s)", tjGetErrorStr());
    REPORT(tjDestroy(tjh), TjDestroy, "(%s)", tjGetErrorStr());

    return 0;
}


int save_image_jpeg(byte *im, int index) {
    int ret;
    unsigned long imjl;
    byte *imj;
    tjhandle tjh;


    imjl = 200000;
    imj = tjAlloc((int) imjl);
    tjh = tjInitCompress();
    ret = tjCompress2(tjh, im, config.cols, 3 * config.cols, config.rows, TJPF_BGR, &imj, &imjl, TJSAMP_444, 100, 0);
    REPORT(ret, TjCompress, "(%s)", tjGetErrorStr());
    REPORT(tjDestroy(tjh), TjDestroy, "(%s)", tjGetErrorStr());
    char filename[30];
    sprintf(filename, "./phs/ph_%d.jpg", index);
    FILE *f = fopen(filename, "wb");
    REPORT(f == NULL, GenericFile, "(fopen error)");
    ret = fwrite(imj, 1, imjl, f);
    REPORT(ret < imjl, GenericFile, "(fwrite error: %d instead of %lu)", ret, imjl);
    REPORT(fclose(f), GenericFile, "(fclose error)");

	tjFree(imj);


    return 0;
}


void draw_bbox(byte *im, box b, byte color[3]) {
    int w = config.cols;
    int h = config.rows;


    int left  = (b.x-b.w/2.)*w;
    int right = (b.x+b.w/2.)*w;
    int top   = (b.y-b.h/2.)*h;
    int bot   = (b.y+b.h/2.)*h;


    int thickness = 2; //border width in pixel minus 1
    if(left < 0) left = thickness;
    if(right > w-thickness) right = w-thickness;
    if(top < 0) top = thickness;
    if(bot >= h-thickness) bot = h-thickness;

    int top_row = 3*top*w;
    int bot_row = 3*bot*w;
    int left_col = 3*left;
    int right_col = 3*right;
    for(int k = left_col; k < right_col; k+=3) {
        for(int wh = 0; wh <= thickness; wh++) {
            int border_line = wh*w*3;
            memcpy(&im[k + top_row - border_line], color, 3);
			//TODO invalid write of size 1 vvvv
            memcpy(&im[k + bot_row + border_line], color, 3);
        }
    }
	int pixel_left = top*3*w + left_col;
	int pixel_right = top*3*w + right_col;
    for(int k = top; k < bot; k++) {
        for(int wh = 0; wh <= thickness*3; wh+=3) {
            memcpy(&im[pixel_left  - wh], color, 3);
            memcpy(&im[pixel_right + wh], color, 3);
        }
		pixel_left += 3*w;
		pixel_right += 3*w;
    }
}



int elaborate_packet(RecvPacket *packet, int rl, SendPacket *sbuffer) {

	REPORT(packet->stx != STX, PckSTX, "(%d != %d).", packet->stx, STX);


	REPORT(packet->l + OH_SIZE > rl, PckTooSmall, "(%d > %d).", packet->l + OH_SIZE, rl);

	sbuffer->stx = STX;
	sbuffer->index = packet->index;
	nn->bboxes = sbuffer->bboxes;

	sbuffer->n = elaborate_image(packet->image, packet->l, sbuffer->index);

	return sbuffer->n;
}


int elaborate_image(byte *imbuffer, int iml, int index) {
	int nbbox;

	byte *im;
	if(!config.isBMP) {
	    byte im_bmp[image_size];
        decode_jpeg(imbuffer, iml, im_bmp);
        im = im_bmp;
	}
    else {
        im = imbuffer;
    }

	nn->input = (float *) im;

	nbbox = ncs_inference(3);

    //byte color[3] = {250, 0, 0};
	for(int i = nbbox-1; i >= 0; --i) {
		printf("\t(%7.6f, %7.6f, %7.6f, %7.6f), o=%7.6f, p=%7.6f:\t\t%s\n",
			nn->bboxes[i].box.x, nn->bboxes[i].box.y, nn->bboxes[i].box.w, nn->bboxes[i].box.h, nn->bboxes[i].objectness, nn->bboxes[i].prob, nn->classes[nn->bboxes[i].cindex]);

		//draw_bbox(im, robj[i].box, color);
	}

	if(nbbox >= 0) {
#ifdef OPENCV
		IplImage *iplim = cvCreateImage(cvSize(config.cols, config.rows), IPL_DEPTH_8U, 3);
  		memcpy(iplim->imageData, im, config.cols*config.rows*3);
		cvShowImage("bibo", iplim);
		cvUpdateWindow("bibo");
		// cvWaitKey(10);
	    cvReleaseImage(&iplim);
#endif
		// show_image_cv(im, "bibo2", config.rows, config.cols, 0);
        save_image_jpeg(im, index);
	}

	return nbbox;
}	


int recv_config() {
	int ret;
    int c;
    int cl = 12;
    byte cbuf[cl];

	ret = socket_recv((byte *) &config, sizeof(Config), 0);//REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	// ret = socket_recv((byte*) &config, sizeof(Config), 0);//REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	
	REPORT_ERRNO(ret < 0, -1, "");
	REPORT(ret == 0, -1, "No config received");

	// printf("%d\t%dx%d, %s\n", config.STX, config.rows, config.cols, config.isBMP ? "BMP" : "JPEG");

	buffer_size = (OH_SIZE + config.rows * config.cols * 3) >> (config.isBMP ? 0 : 4);
	image_size = config.rows * config.cols * 3;

	// printf("Buffer size set to %d.\n", buffer_size);

    c = nn->nclasses;
    cl = sizeof(nn->classes);
    memcpy(cbuf,		&config.STX, 	4);
    memcpy(cbuf + 4,	&cl, 			4);
    memcpy(cbuf + 8,	&c, 	4);

    ret = socket_send(cbuf, 12, 0);
	REPORT_ERRNO(ret < 0, -1, "");

	nn->im_cols = config.cols;
	nn->im_rows = config.rows;

	return ret;
}

int recv_images() {
	int nodata = 0;
	RecvPacket *rbuffer = calloc(buffer_size, 1);
	SendPacket sbuffer;

	while(1) {
		int rl = socket_recv((byte*) rbuffer, buffer_size, config.isBMP ? MSG_WAITALL : 0);
		if(rl >= 0 ) {
			printf("RECV:\t%5d\t%5d | %5d\n", rl, rbuffer->index, rbuffer->l);
			if(rl > 0) {
				int nbbox = elaborate_packet(rbuffer, rl, &sbuffer);
				if(nbbox >= 0) {
					int sl = socket_send((byte *) &sbuffer, 12 + nbbox * sizeof(bbox), 0);
					if(!sl) {
						printf("SEND:\t%-5d\t%5d | %5d\n\n", sl, sbuffer.index, sbuffer.n);
						continue;
					}
				}
			} else
			if(rl == 0) {
				if(++nodata == 10) break;
				printf("[Warning %s::%d]: recv return 0.\n", __FILE__, __LINE__);
			}
		}
		else {
			break;
		}
		usleep(1000000);
	}
	free(rbuffer);
	return -1;
}


int run(const char *iface, const char *graph, const char *meta, int port) {
	int ret;


	printf("\nNCS initialization...");
	if(ncs_init(graph, meta, NCSNN_YOLOv2, nn)) exit(1);
	nn = calloc(1, sizeof(nnet));
	printf("OK\n");

	printf("\nSocket is starting...");
	if(socket_start_server(iface, port)) exit(1);
	printf("OK\n");


	nn->thresh = thresh;

	ret = INT32_MAX;
	while(1) {
		if(ret != INT32_MAX) {
			if(socket_read_close()) {
				break;
			}
		}

		printf("\nSocket is waiting new incoming tcp connection at port %d...", port);
		ret = socket_wait_connection();
        if(ret < 0) continue;
		printf("OK\n");

		printf("\nSocket is receiving config parameters...");
		ret = recv_config();
        if(ret < 0) continue;
		printf("OK\n");


		printf("\nClient image parameters: rows=%d, cols=%d, isBMP=%d.\n", config.rows, config.cols, config.isBMP);

		printf("Receiving started...\n");
		ret = recv_images();
        if(ret < 0) continue;
		else break;
	}

	printf("\n\nClosing all...");

	socket_close();

	ncs_destroy();

	return 0;
}

int main (int argc, char** argv) {
    
	setvbuf(stdout, NULL, _IONBF, 0);


    char *graph = "./yolov2/original/yolov2-tiny-original.graph";
    char *meta = "./yolov2/original/yolov2-tiny-original.meta";
    char *iface = "wlan0";
    unsigned int port = 56789;


	int r;
	srand(time(0)); 
	r = rand() % 1000;
	port = r + 50000;


	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("HoloOj for Raspberry.\n\t--graph\t\t\tthe path to the graph.\n\t--iface\t\t\tthe network interface to use.\n\t--help, -h\t\tthis help.\n");
		exit(0);
	}

	if(argc >= 3) {
		for(int i = 1; i < argc; i++) {
			if(!strcmp(argv[i], "--iface")) {
				iface = argv[i+1];
			}
			if(!strcmp(argv[i], "--graph")) {
				graph = argv[i+1];
			}
			if(!strcmp(argv[i], "--meta")) {
				meta  = argv[i+1];
			}
			if(!strcmp(argv[i], "--port")) {
				port  = atoi(argv[i+1]);
			}
		}
	}


    printf("HoloOj for Raspberry:\n\t%6s = %s\n\t%6s = %u\n\t%6s = %s\n\t%6s = %s\n",
	"iface", iface, "port", port, "graph", graph, "meta", meta);

    run(iface, graph, meta, port);

}