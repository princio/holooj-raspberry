/*
 * socket.cpp
 *
 *  Created on: Mar 7, 2019
 *      Author: developer
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <turbojpeg.h>
#include <unistd.h>

#include "socket.h"
#include "ny2.h"

#define PORT_IN 56789
#define QUEUE_LENGTH 3

#define MOV
#define SHOWIMAGE

#define RECV 1
#define SEND 0

#define RECV_CONTINUE { usleep(1000); continue; }

const int STX = 767590;
const int OH_SIZE = 12;

typedef struct Config {
	int STX;
	int width;
	int height;
	int isBMP;
} Config;

Config config;

int socket_errno;
int buffer_size;
int image_size;
int sockfd_server;
int sockfd_read;
struct pollfd ufds[1];
struct sockaddr_in server_addr;
struct sockaddr_in holo_addr;
struct timeval tv;
struct timeval select_timer;
float thresh = 0.5;
int timeouts = 0;

char iface[10] = "enp0s9";



int elaborate_packet(RecvBuffer *rbuffer, int rl, SendBuffer *sbuffer);
int elaborate_image(byte *imbuffer, int iml, rec_object dets[5], int index);
int socket_wait_data(int rec);


int get_address(in_addr_t *addr, char *iface) {

	struct ifaddrs *ifaddr, *ifa;
	RIFE(-1 == getifaddrs(&ifaddr), SO, Addr, "");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		int condition = !strcmp(ifa->ifa_name, iface) && ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET);
	    if (condition) {
			printf("addr = %s\n", inet_ntoa(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr));
	        *addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
	        break;
	    }
	}
	freeifaddrs(ifaddr);

	return 0;
}


int decode_jpeg(byte *imj, int imjl, byte *im) {
    tjhandle tjh;
    int w, h, ss, cl, ret;
    
    tjh = tjInitDecompress();
		
    if(tjDecompressHeader3(tjh, imj, imjl, &w, &h, &ss, &cl)) {
        RIFE(1, Tj, Header, "(%s)", tjGetErrorStr());
    }
    switch(cl) {
        case TJCS_CMYK: printf("CMYK"); break;
        case TJCS_GRAY: printf("GRAY"); break;
        case TJCS_RGB: printf("RGB"); break;
        case TJCS_YCbCr: printf("YCbCr"); break;
        case TJCS_YCCK: printf("YCCK"); break;
    }
	RIFE(cl != TJCS_RGB, Im, UnsopportedColorSpace, "");
	ret = tjDecompress2(tjh, imj, imjl, im, 0, 0, 0, TJPF_RGB, 0);
	RIFE(ret, Tj, Decompress, "(%s)", tjGetErrorStr());
    RIFE(tjDestroy(tjh), Tj, Destroy, "(%s)", tjGetErrorStr());

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
    ret = tjCompress2(tjh, im, config.width, 0, config.height, TJPF_RGB, &imj, &imjl, TJSAMP_444, 100, 0);
    RIFE(ret, Tj, Compress, "(%s)", tjGetErrorStr());
    RIFE(tjDestroy(tjh), Tj, Destroy, "(%s)", tjGetErrorStr());

    char filename[30];
    sprintf(filename, "./phs/ph_%d.jpg", index);
    FILE *f = fopen(filename, "wb");
    RIFE(!f, Generic, File, "(fopen error)");
    ret = fwrite(imj, 1, imjl, f);
    RIFE(ret < imjl, Generic, File, "(fwrite error: %d instead of %lu)", ret, imjl);
    RIFE(fclose(f), Generic, File, "(fclose error)");

    return 0;
}


void draw_bbox(byte *im, box b, byte color[3]) {
    int w = config.width;
    int h = config.height;


    int left  = (b.x-b.w/2.)*w;
    int right = (b.x+b.w/2.)*w;
    int top   = (b.y-b.h/2.)*h;
    int bot   = (b.y+b.h/2.)*h;


    int thickness = 0; //border width in pixel minus 1
    if(left < 0) left = thickness;
    if(right > w-thickness) right = w-thickness;
    if(top < 0) top = thickness;
    if(bot > h-thickness) bot = h-thickness;

    int top_row = 3*top*h;
    int bot_row = 3*bot*h;
    int left_col = 3*left;
    int right_col = 3*right;
    for(int k = left_col; k <= right_col; k+=3) {
        for(int wh = 0; wh <= thickness; wh++) {
            int border_line = wh*w*3;
            memcpy(&im[k + top_row - border_line], color, 3);
            memcpy(&im[k + bot_row + border_line], color, 3);
        }
    }
    for(int k = top_row; k <= bot_row; k+=3) {
        for(int wh = 0; wh < thickness; wh++) {
            memcpy(&im[k - wh + left_col],  color, 3);
            memcpy(&im[k + wh + right_col], color, 3);
        }
    }
}


int socket_start_server() {
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
	RIFE(0>sockfd_server, SO, Creation, "");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_IN);

	if(get_address(&server_addr.sin_addr.s_addr, iface)) return -1;

	RIFE(0>bind(sockfd_server, (struct sockaddr *) &server_addr, sizeof(server_addr)), SO, Bind, "");

	printf("start listening on port %d.\n", PORT_IN);

	RIFE(0>listen(sockfd_server, 3), SO, Listening, "");

	return 0;
}


int socket_wait_connection() {
    int addrlen = sizeof(holo_addr);
	timeouts = 0;
	
	printf("waiting new connection...");
	sockfd_read = accept(sockfd_server, (struct sockaddr *)&holo_addr, (socklen_t*)&addrlen);
	RIFE(0 > sockfd_read, SO, Accept, "");
	printf("Connected!\n");

	ufds[0].fd = sockfd_read;
	ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band

	return 0;
}


int socket_recv_config() {
	int ret;

	if(socket_wait_data(RECV)) return -1;

	ret = recv(sockfd_read, &config, sizeof(Config), 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	RIFE(ret <= 0, SO, Recv, "(%d)", ret);

	printf("%d\t%dx%d, %s\n", config.STX, config.width, config.height, config.isBMP ? "BMP" : "JPEG");

	buffer_size = (OH_SIZE + config.width*config.height*3) >> (config.isBMP ? 0 : 4);
	image_size = config.width*config.height*3;

	printf("Buffer size set to %d.\n", buffer_size);

	if(socket_wait_data(SEND)) return -1;
	ret = send(sockfd_read, &config.STX, 4, 0);

	return 0;
}


int socket_wait_data(int rec) {
	int rv = poll(ufds, 1, 100);
	RIFE(rv == -1, Poll, Generic, "");
	if(rv==0) ++timeouts;
	RIFE(rv == 0,  Poll, Timeout, "");
	if(rec) {
		rv = ufds[0].revents | POLLIN;
		RIFE(!rv,  Poll, In, "");
	} else {
		rv = ufds[0].revents | POLLOUT;
		RIFE(!rv,  Poll, Out, "");
	}
	// printf("%s to %s\n", !rv ? "not ready" : "ready", rec == RECV ? "receiving." : "sending.");
	return !rv;
}


int socket_receiving() {
	// struct timespec sleep_time;
	// sleep_time.tv_nsec=1*000*000; //1 ms

	RecvBuffer *rbuffer = calloc(buffer_size, 1);
	SendBuffer sbuffer;

	printf("Receiving started.\n");
	while(1) {
		int rl;

		if(!socket_wait_data(RECV)) {
			rl = recv(sockfd_read, rbuffer, buffer_size, config.isBMP ? MSG_WAITALL : 0);
			if(rl > 0) {
				int nbbox = elaborate_packet(rbuffer, rl, &sbuffer);
				if(nbbox >= 0) {
					if(!socket_wait_data(SEND)) {
						int sl = send(sockfd_read, &sbuffer, 8 + nbbox*sizeof(rec_object), 0);
						if(sl <= 0) {
							printf("\n[%s::%d]\t""SO"" ""Send""\t(errno#%d=%s)""(%d)"".\n\n", __FILE__, __LINE__, errno, strerror(errno),sl);
							socket_errno = (SOError) SOSend;
						}
						else continue;
					}
				}
			} else if(rl == 0) {
				printf("[Warning %s::%d]: recv return 0.\n", __FILE__, __LINE__);
			} else {
				printf("\n[%s::%d]\t""SO"" ""Send""\t(errno#%d=%s)""(%d)"".\n\n", __FILE__, __LINE__, errno, strerror(errno), rl);
				socket_errno = (SOError) SOSend;
			}
		}
		usleep(1000000);
	}
}


int elaborate_packet(RecvBuffer *packet, int rl, SendBuffer *sbuffer) {

	RIFE(packet->stx != STX, Pck, STX, "(%d != %d).", packet->stx, STX);


	RIFE(packet->l + OH_SIZE > rl, Pck, TooSmall, "(%d > %d).", packet->l + OH_SIZE, rl);

	sbuffer->stx = STX;
	sbuffer->index = packet->index;
	sbuffer->n = elaborate_image(packet->image, packet->l, sbuffer->dets, sbuffer->index);

	printf("##\t%-5d B\tindex=%-4d\tnbbox=%-d\r", packet->l, packet->index, sbuffer->n);

	return sbuffer->n;
}

int elaborate_image(byte *imbuffer, int iml, rec_object robj[5], int index) {
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

	nbbox = ny2_inference_byte(im, robj, thresh, config.width, config.height, 3);

    byte color[3] = {250, 0, 0};
	for(int i = nbbox-1; i >= 0; --i) {
		printf("%5d#%d) x=%7.6f | y=%7.6f | w=%7.6f | h=%7.6f\to=%7.6f | p=%7.6f\t\t%s\n",
			index, nbbox - i,
			robj[i].bbox.x, robj[i].bbox.y, robj[i].bbox.w, robj[i].bbox.h, robj[i].objectness, robj[i].prob, robj[i].name);

		draw_bbox(im, robj[i].bbox, color);
	}
	printf("\n");

	if(nbbox >= 0) {
        save_image_jpeg(im, index);
	}

	return nbbox;
}	


int main( int argc, char** argv )
{
    int ret;
	setvbuf(stdout, NULL, _IONBF, 0);

	make_window("bibo", 512, 512);

#ifdef SOCKET
	if(socket_start_server()) exit(socket_errno);
	if(argc < 2) {
		printf("Missing interface name: set to ");
	} else {
		strcpy(iface, argv[1]);
		printf("Interface set to ");
	}
	printf("<<%s>>.\n", iface);

	if(ny2_init()) exit(ret);

	ret = 0;
	while(1) {
		if(ret) {
			if(close(sockfd_read)) {
				printf("\nError during closing [errno=%d].\n", errno);
				break;
			}
		}
		ret = socket_wait_connection(); if(ret) continue;
		ret = socket_recv_config(); 	if(ret) continue;
		ret = socket_receiving();		if(ret) continue;
	}
#else
	rec_object robj[5];
	char fpath[100] = {0};
	int sz = 100;
	int lr;
	printf("Insert photo path: ");
	if (fgets (fpath, sz, stdin) == NULL) exit(-1);
	if(fpath[0] = '\n') {
		strcpy(fpath, "/home/developer/dog.jpg");
	}
	printf("\n Elaborating --> %s...", fpath);

	config.isBMP = 1;
	byte *dogbuffer = load_image_cv(fpath, &config.width, &config.height, &lr);

	// unsigned int lr = 0;
	// FILE*dog = fopen(fpath, "rb");
    // if(dog == NULL) return -1;

    // fseek(dog, 0, SEEK_END);
    // lr = ftell(dog);
    // rewind(dog);
	// byte dogbuffer[lr];
    // size_t read_count = fread(dogbuffer, 1, lr, dog);
	// fclose(dog);

	if(ny2_init()) exit(ret);
	elaborate_image(dogbuffer, lr, robj, 0);
#endif
	ny2_destroy();

	return 0;
}
