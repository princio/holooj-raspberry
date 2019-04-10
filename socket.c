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
	test_return(getifaddrs(&ifaddr) == -1, StsError, "Socket creation failed.");

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


int socket_init() {

#ifdef TCP
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
#else
	sockfd_server = socket(AF_INET, SOCK_DGRAM, 0);
#endif

	test_return(sockfd_server < 0, sockfd_server, "Socket creation failed.");

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_IN);


	get_address(&addr.sin_addr.s_addr, iface);

	// Forcefully attaching socket to the port PORTIN
	test_return(bind(sockfd_server, (struct sockaddr *)&addr, sizeof(addr))<0, sockfd_server, "Bind failed.");

	printf("start listening on port %d.\n", PORT_IN);

	test_return(listen(sockfd_server, 3) < 0, sockfd_server, "Listening error.");

	return 0;
}



int socket_wait_connection() {
    int addrlen = sizeof(addr);
	timeouts = 0;
	
	printf("waiting new connection...");
	sockfd_read = accept(sockfd_server, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
	test_return(sockfd_read < 0, sockfd_read, "Error during accepting.");
	printf("Connected!\n");

	ufds[0].fd = sockfd_read;
	ufds[0].events = POLLIN | POLLPRI; // check for normal or out-of-band

	// setsockopt(sockfd_read, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));

	recv(sockfd_read, &config, sizeof(Config), 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.

	printf("%s\t%dx%d, %d\n", config.hello, config.width, config.height, config.isBMP);

	buffer_size = (OH_SIZE + config.width*config.height*3) >> (config.isBMP ? 0 : 4);
	image_size = config.width*config.height*3;

	printf("Buffer size set to %d  .\n", buffer_size);

	return 0;
}

int socket_check_connection() {

	int rv = poll(ufds, 1, 100);

	test_return(rv == -1, SockPoolError, "Polling: some error [errno=%d].", errno);

	test_return(rv == 0, SockPoolTimeout, "Pooling: timeout occurred, no data after 100 ms.\n");

	// if(ufds[0].revents | POLLIN)
	//   printf("Pooling: POLLIN ok.\n");
	return 0;
}



int socket_receiving() {
	struct timespec sleep_time;
	sleep_time.tv_nsec=1*000*000; //1 ms

	byte rbuffer[buffer_size];
	byte sbuffer[100];

	printf("Receiving started.\n");

	printf("Receiving...");
	while(1) {
		int rl;

		switch(socket_check_connection()) {
			case 0:
			break;
			case SockPoolError:
				return SockPoolError;
			case SockPoolTimeout:
				if(++timeouts < 100) continue;
				return SockPoolTimeout;
		}

		rl = recv(sockfd_read, rbuffer, buffer_size, config.isBMP ? MSG_WAITALL : 0);
		if(rl > 0) {
			int sl;

			printf("get %d bytes.\n", rl);

			printf("%hhu %hhu %hhu %hhu\n", rbuffer[0], rbuffer[1], rbuffer[2], rbuffer[3]);
			
			if(!elaborate_packet(rbuffer, rl, sbuffer, &sl)) {
				if((sl = send(sockfd_read, sbuffer, sl, 0)) <= 0) {
					ERROR("Sending error (sent %d)", sl);
				}
			}

		} else{
			test_return(rl < 0, SockRecvError, "Receiving error (%d)", errno);
		} 
	}
}


int elaborate_packet(byte *packet, int rl, byte *sbuffer, int *sl) {

	int l, index;

	int stx = (int) *((int*) packet);
	test_return(stx != STX, PckSTX, "Wrong STX (%d != %d).", stx, STX);

	l = *((int*) &packet[4]);

	test_return(l + OH_SIZE > rl, PckSmall, "Packet too small: received %d.", rl);

	index = *((int*) &packet[8]);
	printf("Size of %d bytes, index=%d", l, index);

	memcpy(sbuffer, &STX, 4);
	memcpy(&sbuffer[8], &packet[8], 4); //index

	return elaborate_image(packet + OH_SIZE, rl - OH_SIZE, &sbuffer[12], (int*) &sbuffer[4], index);
}


int elaborate_image(byte *imbuffer, int iml, byte *bbox_buffer, int *bl, int index) {
	int w, h, ss, cl;

	unsigned char im[image_size];

	if(!config.isBMP) {
		tjhandle tjh = tjInitDecompress();
		if(tjDecompressHeader3(tjh, imbuffer, iml, &w, &h, &ss, &cl)) {
			printf("\n%s\n", tjGetErrorStr());
			return -1;
		}
		switch(cl) {
			case TJCS_CMYK: printf("CMYK"); break;
			case TJCS_GRAY: printf("GRAY"); break;
			case TJCS_RGB: printf("RGB"); break;
			case TJCS_YCbCr: printf("YCbCr"); break;
			case TJCS_YCCK: printf("YCCK"); break;
		}
		tjDecompress2(tjh, imbuffer, iml, im, 0, 0, 0, TJPF_RGB, 0);
		tjDestroy(tjh);
		char filename[30];
		sprintf(filename, "../ph_%d.bmp", index);
		FILE *f = fopen(filename, "wb");
		fwrite(imbuffer, 1, iml, f);
		fclose(f);
	}
	else {
		unsigned long imjl = 65000;
		byte *imj = tjAlloc((int) imjl);
		tjhandle tjh = tjInitCompress();
		int ret = tjCompress2(tjh, imbuffer, 416, 0, 416, TJPF_RGB, &imj, &imjl, TJSAMP_444, 100, 0);
		test(ret < 0, ImTjError, "Error during compressing.");
		tjDestroy(tjh);
		char filename[30];
		sprintf(filename, "../ph_%d.jpg", index);
		FILE *f = fopen(filename, "wb");
		test(!f, 0, "Error during file <%s> opening.", filename);
		int fw = fwrite(imj, 1, imjl, f);
		test(fw < imjl, 0, "Error during file writing: %d < %lu", fw, imjl);
		fclose(f);
	}

    if(iml != yolo_input_size) {
        return -1;
    }

	int i = 0;
	while(i <= (iml-3)) {
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
	}

	// X[i] = imbuffer[i+2] / 255.; ++i;
	// X[i] = imbuffer[i] / 255.;   ++i;
	// X[i] = imbuffer[i-2] / 255.; ++i;

	int a = mov_inference((float *) bbox_buffer, bl, thresh);

    // printf("%d#%s\t(%7.6f, %7.6f, %7.6f, %7.6f, %7.6f)\n", *(int*) &bbox_buffer[20], (char*) &bbox_buffer[24], *(float*)&bbox_buffer[0], *(float*)&bbox_buffer[4], *(float*)&bbox_buffer[8], *(float*)&bbox_buffer[12], *(float*)&bbox_buffer[16]);
    // printf("x=%7.6f y=%7.6f w=%7.6f h=%7.6f\tobj=%7.6f cls=%7.6f\t%d, %s\n",
    printf("    x        y        w        h       obj        cls    \n");
    printf("%7.6f | %7.6f | %7.6f | %7.6f\t%7.6f | %7.6f\t%d, %s\n",
		*(float*) &bbox_buffer[0], *(float*) &bbox_buffer[4], 
		*(float*)&bbox_buffer[8],  *(float*)&bbox_buffer[12],
		*(float*)&bbox_buffer[16], *(float*)&bbox_buffer[20],
		*(int*)&bbox_buffer[24],   (char*)&bbox_buffer[28]);
	return a;
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

    if(socket_init()) {
		exit(1);
	}

	while(1) {
		socket_wait_connection();
		socket_receiving();
		if(!close(sockfd_read)) {
			printf("\nError during closing [errno=%d].\n", errno);
			exit(1);
		}
	}
#endif

	mov_destroy();

	return 0;
}
