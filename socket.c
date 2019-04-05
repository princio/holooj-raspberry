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
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <turbojpeg.h>

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
	int buffer_size;
	int isBMP;
} Config;

Config config;

int sockfd_server;
int sockfd_read;
struct sockaddr_in addr;
struct timeval tv;
float thresh = 0.3;

char iface[10] = "wlan0";


int elaborate_packet(byte *rbuffer, int rl, byte *sbuffer, int *sl);
int elaborate_image(byte *imbuffer, int iml, byte *bbox_buffer, int *bl);


int socket_init() {

#ifdef TCP
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
#else
	sockfd_server = socket(AF_INET, SOCK_DGRAM, 0);
#endif

	if(sockfd_server < 0){
		printf("socket creation failed: errno=(%d)\n", sockfd_server);
		return sockfd_server;
	}

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_IN);


	struct ifaddrs *ifa, *_ifa;
	if (getifaddrs(&ifa) == -1) {
	    perror("getifaddrs failed");
	    return -1;
	}
	_ifa = ifa;
	while (_ifa) {
		int condition = !strcmp(_ifa->ifa_name, iface) && _ifa->ifa_addr && (_ifa->ifa_addr->sa_family == AF_INET);
	    if (condition) {
	        printf("addr = %s\n", inet_ntoa(((struct sockaddr_in *)_ifa->ifa_addr)->sin_addr));
	        addr.sin_addr.s_addr = ((struct sockaddr_in *)_ifa->ifa_addr)->sin_addr.s_addr;
	        break;
	    }
	    _ifa = _ifa->ifa_next;
	}
	freeifaddrs(ifa);


	// Forcefully attaching socket to the port PORTIN
	if (bind(sockfd_server, (struct sockaddr *)&addr, sizeof(addr))<0) {
		printf("bind failed\n");
		return -1;
	}

#ifdef TCP
    int addrlen = sizeof(addr);
	printf("start listening on port %d.\n", PORT_IN);
    if (listen(sockfd_server, 3) < 0) 
    { 
        printf("listen\n"); 
		return -1;
    } 
	printf("waiting new connection...");
    if ((sockfd_read = accept(sockfd_server, (struct sockaddr *)&addr,  
                       (socklen_t*)&addrlen))<0) 
    { 
        printf("accept\n"); 
		return -1;
    }


	printf("Connected!\n");


	// setsockopt(sockfd_read, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));

	recv(sockfd_read, &config, sizeof(Config), 0); //REMEMBER ALIGNMENT! char[6] equal to char[8] because of it.

	printf("%s\t%dx%d, %d, %d", config.hello, config.width, config.height, config.buffer_size, config.isBMP);


#endif
	return 0;
}





void socket_receiving() {
	struct timespec sleep_time;
	sleep_time.tv_nsec=1*000*000; //1 ms
	int sock_ph_n = 1;

	byte rbuffer[config.isBMP ? config.width*config.height*3*sock_ph_n : config.width*config.height*3*sock_ph_n  >> 4];
	byte sbuffer[100];

	printf("Receiving started.\n");

	while(1) {
		int rl;
		printf("Receiving...");
		if(config.isBMP) {
			rl = recv(sockfd_read, rbuffer, config.buffer_size, MSG_WAITALL);
		} else {
			rl = recv(sockfd_read, rbuffer, config.buffer_size, 0);
		}

		// if(pointer[])
		if(rl > 0) {
			printf("get %d bytes.\n", rl);


			int sl;
			
			elaborate_packet(rbuffer, rl, sbuffer, &sl);


			send(sockfd_read, sbuffer, sl, 0);
		} else{
			if (rl < 0){
				printf("error (%d), errno = %d.\n", rl, errno);
			}
			nanosleep(&sleep_time, NULL);
		} 

		// if(rl > 0 && rl < MAX_PHOTO_SIZE) {
		// 	int idx = numPhoto % 3;
		// 	memcpy(&ph_lengths[idx], pointer, 4);
		// 	memcpy(&ph_indexes[idx], pointer + 4, 4);
		// 	++numPhoto;
		// 	pointer = circularBuffer + ((idx+1)%3) * MAX_PHOTO_SIZE;
		// }
		// else
		// 	printf("none: %d\n", rl);
	}
}


int elaborate_packet(byte *packet, int rl, byte *sbuffer, int *sl) {

	int l, index;

	if((int) *((int*) packet) != STX) return -1;

	l = *((int*) &packet[4]);
	if(l + OH_SIZE < rl) return -1;

	index = *((int*) &packet[8]);

	printf("Size of %d bytes, index=%d", l, index);


	int w, h, ss, cl;


	tjhandle tjh = tjInitDecompress();
	if(tjDecompressHeader3(tjh, packet + 12, rl - OH_SIZE, &w, &h, &ss, &cl)) {
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

	int size = w*h*3;
	unsigned char im[size];


	tjDecompress2(tjh, packet + 12, rl - OH_SIZE, im, 0, 0, 0, TJPF_RGB, 0);


	memcpy(sbuffer, &STX, 4);
	memcpy(&sbuffer[8], &packet[8], 4); //index


	elaborate_image(im, size, &sbuffer[12], (int*) &sbuffer[4]);

	return;
}

int elaborate_image(byte *imbuffer, int iml, byte *bbox_buffer, int *bl) {

    if(iml != yolo_input_size) {
        return -1;
    }

	int i = -1;
	while(i <= (iml-3)) {
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
		yolo_input[i] = imbuffer[i] / 255.; ++i;
	}

	// X[i] = imbuffer[i+2] / 255.; ++i;
	// X[i] = imbuffer[i] / 255.;   ++i;
	// X[i] = imbuffer[i-2] / 255.; ++i;

	int a = mov_inference((float *) bbox_buffer, bl, thresh);

    printf("%d#%s\t(%7.6f, %7.6f, %7.6f, %7.6f, %7.6f)\n", *(int*) &bbox_buffer[20], (char*) &bbox_buffer[24], *(float*)&bbox_buffer[0], *(float*)&bbox_buffer[4], *(float*)&bbox_buffer[8], *(float*)&bbox_buffer[12], *(float*)&bbox_buffer[16]);
	return a;
}	


int main( int argc, char** argv )
{
	setvbuf(stdout, NULL, _IONBF, 0);

	mov_init();


	FILE*f = fopen("/home/developer/dog.jpg", "rb");


	fseek(f, 0, SEEK_END);
	unsigned long l = (unsigned long) ftell(f);
	fseek(f, 0, SEEK_SET);


	byte packet[l + OH_SIZE];
	memcpy(&packet[0], &STX, 4);
	memcpy(&packet[4], &l, 4);
	int index = 897;
	memcpy(&packet[8], &index, 4);


	fread(&packet[OH_SIZE], l, 1, f);
	fclose(f);

	int sl;
	byte sbuffer[100];
	elaborate_packet(packet, l + OH_SIZE, sbuffer, &sl);


	return 0;


	if(argc < 2) {
		printf("Missing interface name: set to ");
	} else {
		strcpy(iface, argv[1]);
		printf("Interface set to ");
	}
	printf("<<%s>>.\n", iface);

    if(socket_init()) {
		printf("Error: socket initialization.");
		exit(1);
	}

    if(mov_init()) {
		printf("Error: movidius initialization.");
		exit(1);
	}
}
