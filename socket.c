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
#include "movidius.h"
#include "yolo.h"

#define PORT_IN 56789
#define QUEUE_LENGTH 3

#define TCP

const int STX = 767590;
const int OH_SIZE = 12;

typedef struct Config {
	char hello[8];
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
struct sockaddr_in addr;
struct timeval tv;
struct timeval select_timer;
float thresh = 0.3;
int timeouts = 0;

char iface[10] = "enp0s9";


int elaborate_packet(byte *rbuffer, int rl, byte *sbuffer, int *sl);
int elaborate_image(byte *imbuffer, int iml, byte *bbox_buffer, int *bl, int index);


int get_address(in_addr_t *addr, char *iface) {

	struct ifaddrs *ifaddr, *ifa;
	RIFE(getifaddrs(&ifaddr) == -1, SO, Addr, "");

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


int socket_start_server() {
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);

	RIFE(sockfd_server, SO, Creation, "");

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_IN);

	ifget_address(&addr.sin_addr.s_addr, iface);

	RIFE(bind(sockfd_server, (struct sockaddr *) &addr, sizeof(addr)), SO, Bind, "");

	printf("start listening on port %d.\n", PORT_IN);

	RIFE(listen(sockfd_server, 3), SO, Listening, "");

	return 0;
}



int socket_wait_connection() {
    int addrlen = sizeof(addr);
	timeouts = 0;
	
	printf("waiting new connection...");
	sockfd_read = accept(sockfd_server, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
	RIFE(sockfd_read, SO, Accept, "");
	printf("Connected!\n");

	ufds[0].fd = sockfd_read;
	ufds[0].events = POLLIN | POLLPRI; // check for normal or out-of-band
}


int socket_recv_config() {
	int ret;

	if(socket_wait_data()) return -1;

	ret = recv(sockfd_read, &config, sizeof(Config), 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.
	RIFE(ret <= 0, SO, Recv, "(%d)", ret);

	printf("%s\t%dx%d, %d\n", config.hello, config.width, config.height, config.isBMP);

	buffer_size = (OH_SIZE + config.width*config.height*3) >> (config.isBMP ? 0 : 4);
	image_size = config.width*config.height*3;

	printf("Buffer size set to %d  .\n", buffer_size);

	return 0;
}


int socket_wait_data() {
	int rv = poll(ufds, 1, 100);
	RIFE(rv == -1, Poll, Generic, "");
	if(rv==0) ++timeouts;
	RIFE(rv == 0,  Poll, Timeout, "");
	return 0;
	// if(ufds[0].revents | POLLIN) printf("Pooling: POLLIN ok.\n");
}



int socket_receiving() {
	struct timespec sleep_time;
	sleep_time.tv_nsec=1*000*000; //1 ms

	byte rbuffer[buffer_size];
	byte sbuffer[100];

	printf("Receiving started.\n");
	while(1) {
		int rl;

		if(socket_wait_data()) return -1;

		rl = recv(sockfd_read, rbuffer, buffer_size, config.isBMP ? MSG_WAITALL : 0);
		if(rl > 0) {
			int sl;

			printf("get %d bytes.\n", rl);
			printf("%hhu %hhu %hhu %hhu\n", rbuffer[0], rbuffer[1], rbuffer[2], rbuffer[3]);
			
			if(elaborate_packet(rbuffer, rl, sbuffer, &sl)) {
				memcpy(&sbuffer[4], -1, 4); // if there is some error index is set to -1.
			} else {
				sl = send(sockfd_read, sbuffer, sl, 0);
			}


			RIFE(sl, SO, Send, "(%d)", sl);
		} else if(rl == 0) {
			printf("[Warning %s::%d]: recv return 0.", __FILE__, __LINE__);
		} else {
			RIFE(rl < 0, SO, Recv, "");
		} 
	}
}


int elaborate_packet(byte *packet, int rl, byte *sbuffer, int *sl) {

	int l, index;

	int stx = (int) *((int*) packet);
	RIFE(stx != STX, Pck, STX, "(%d != %d).", stx, STX);

	l = *((int*) &packet[4]);

	RIFE(l + OH_SIZE > rl, Pck, TooSmall, "(%d > %d).", l + OH_SIZE, rl);

	index = *((int*) &packet[8]);
	printf("Size of %d bytes, index=%d", l, index);

	memcpy(sbuffer, &STX, 4);
	memcpy(&sbuffer[8], &index, 4); //index

	return elaborate_image(packet + OH_SIZE, rl - OH_SIZE, &sbuffer[12], (int*) &sbuffer[4], index);
}


int elaborate_image(byte *imbuffer, int iml, byte *bbox_buffer, int *bl, int index) {
	int w, h, ss, cl, ret;

	unsigned char im[image_size];

	if(!config.isBMP) {
		tjhandle tjh = tjInitDecompress();
		
		if(tjDecompressHeader3(tjh, imbuffer, iml, &w, &h, &ss, &cl)) {
			RIFE(1, Tj, Header, "(%s)", tjGetErrorStr());
		}
		switch(cl) {
			case TJCS_CMYK: printf("CMYK"); break;
			case TJCS_GRAY: printf("GRAY"); break;
			case TJCS_RGB: printf("RGB"); break;
			case TJCS_YCbCr: printf("YCbCr"); break;
			case TJCS_YCCK: printf("YCCK"); break;
		}
		ret = tjDecompress2(tjh, imbuffer, iml, im, 0, 0, 0, TJPF_RGB, 0);
		RIFE(ret, Tj, Decompress, "(%s)", tjGetErrorStr());
		RIFE(tjDestroy(tjh), Tj, Destroy, "(%s)", tjGetErrorStr());

		char filename[30];
		sprintf(filename, "../ph_%d.bmp", index);
		FILE *f = fopen(filename, "wb");
		RIFE(!f, Gen, _, "(fopen error)");
		int ret = fwrite(imbuffer, 1, iml, f);
		RIFE(ret < iml, Gen, _, "(fwrite error: %d instead of %d)", ret, iml);
		RIFE(fclose(f), Gen, _, "(fclose error)");
	}
	else {
		unsigned long imjl = 65000;
		byte *imj = tjAlloc((int) imjl);
		tjhandle tjh = tjInitCompress();
		ret = tjCompress2(tjh, imbuffer, 416, 0, 416, TJPF_RGB, &imj, &imjl, TJSAMP_444, 100, 0);
		RIFE(ret, Tj, Compress, "(%s)", tjGetErrorStr());
		RIFE(tjDestroy(tjh), Tj, Destroy, "(%s)", tjGetErrorStr());

		char filename[30];
		sprintf(filename, "../ph_%d.jpg", index);
		FILE *f = fopen(filename, "wb");
		RIFE(!f, Gen, _, "(fopen error)");
		ret = fwrite(imbuffer, 1, imjl, f);
		RIFE(ret < imjl, Gen, _, "(fwrite error: %d instead of %lu)", ret, imjl);
		RIFE(fclose(f), Gen, _, "(fclose error)");
	}

	RIFE(iml != yolo_input_size, Im, WrongSize, "(%d != %d)", iml, yolo_input_size);

	int i = 0;
	while(i <= (iml-3)) {
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
	}

	// X[i] = imbuffer[i+2] / 255.; ++i;
	// X[i] = imbuffer[i] / 255.;   ++i;
	// X[i] = imbuffer[i-2] / 255.; ++i;

	ret = mov_inference((float *) bbox_buffer, bl, thresh);
	if(ret) return ret;

    // printf("%d#%s\t(%7.6f, %7.6f, %7.6f, %7.6f, %7.6f)\n", *(int*) &bbox_buffer[20], (char*) &bbox_buffer[24], *(float*)&bbox_buffer[0], *(float*)&bbox_buffer[4], *(float*)&bbox_buffer[8], *(float*)&bbox_buffer[12], *(float*)&bbox_buffer[16]);
    // printf("x=%7.6f y=%7.6f w=%7.6f h=%7.6f\tobj=%7.6f cls=%7.6f\t%d, %s\n",
    printf("    x        y        w        h       obj        cls    \n");
    printf("%7.6f | %7.6f | %7.6f | %7.6f\t%7.6f | %7.6f\t%d, %s\n",
		*(float*) &bbox_buffer[0], *(float*) &bbox_buffer[4], 
		*(float*)&bbox_buffer[8],  *(float*)&bbox_buffer[12],
		*(float*)&bbox_buffer[16], *(float*)&bbox_buffer[20],
		*(int*)&bbox_buffer[24],   (char*)&bbox_buffer[28]);
	
	return 0;
}	


int main( int argc, char** argv )
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if(mov_init()) {
		exit(1);
	}

#ifndef NCS
	FILE*f = fopen("/home/developer/dog.jpg", "rb");
	fseek(f, 0, SEEK_END);
	unsigned long l = (unsigned long) ftell(f);
	fseek(f, 0, SEEK_SET);


	byte packet[l + OH_SIZE];
	memcpy(&packet[0], &STX, 4);
	memcpy(&packet[4], &l, 4);
	int index = 897;
	memcpy(&packet[8], &index, 4);

	image_size = 416*416*3;
	fread(&packet[OH_SIZE], l, 1, f);
	fclose(f);

	int sl;
	byte sbuffer[100] = {0};
	elaborate_packet(packet, l + OH_SIZE, sbuffer, &sl);
#else
	if(argc < 2) {
		printf("Missing interface name: set to ");
	} else {
		strcpy(iface, argv[1]);
		printf("Interface set to ");
	}
	printf("<<%s>>.\n", iface);

    if(socket_start_server()) {
		exit(1);
	}

	while(1) {
		if(socket_start_server())    exit(socket_errno);

		if(socket_wait_connection()) exit(socket_errno);

		if(socket_recv_config())     exit(socket_errno);

		if(socket_receiving()) 		 exit(socket_errno);

		if(!close(sockfd_read)) {
			printf("\nError during closing [errno=%d].\n", errno);
			exit(1);
		}
	}
#endif

	mov_destroy();

	return 0;
}
