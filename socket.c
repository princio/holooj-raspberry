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

#define MOV

#define RECV 1
#define SEND 0

#define RECV_CONTINUE { usleep(1000); continue; }

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
struct sockaddr_in server_addr;
struct sockaddr_in holo_addr;
struct timeval tv;
struct timeval select_timer;
float thresh = 0.3;
int timeouts = 0;

char iface[10] = "enp0s9";


#define ERROR(M1, V1, M2, V2) const error M1##M2##Error = (1 << V1) | (1 << (16+V2));
error Gen_Error  = INT32_MAX;
error SOMask     = 1 <<  1;
error PollMask   = 1 <<  2;
error PckMask    = 1 <<  3;
error TjMask     = 1 <<  4;
error ImMask     = 1 <<  5;
error NCSMask    = 1 <<  6;
error MovMask    = 1 <<  7;


ERROR(   SO,     1,        Creation,     1);
ERROR(   SO,     1,            Bind,     2);
ERROR(   SO,     1,       Listening,     3);
ERROR(   SO,     1,          Accept,     4);
ERROR(   SO,     1,            Send,     5);
ERROR(   SO,     1,            Recv,     6);
ERROR(   SO,     1,            Addr,     7);

ERROR( Poll,     2,         Generic,     1);
ERROR( Poll,     2,         Timeout,     2);
ERROR( Poll,     2,        RecvBusy,     3);
ERROR( Poll,     2,              In,     4);
ERROR( Poll,     2,             Out,     5);

ERROR(  Pck,     3,           Error,     1);
ERROR(  Pck,     3,             STX,     2);
ERROR(  Pck,     3,        TooSmall,     3);

ERROR(   Tj,     4,         Generic,     1);
ERROR(   Tj,     4,          Header,     2);
ERROR(   Tj,     4,      Decompress,     3);
ERROR(   Tj,     4,        Compress,     4);
ERROR(   Tj,     4,         Destroy,     5);

ERROR(   Im,     5,       WrongSize,     1);

ERROR(  NCS,     6,       DevCreate,     1);
ERROR(  NCS,     6,         DevOpen,     2);
ERROR(  NCS,     6,     GraphCreate,     3);
ERROR(  NCS,     6,   GraphAllocate,     4);
ERROR(  NCS,     6,       Inference,     5);
ERROR(  NCS,     6,          GetOpt,     6);
ERROR(  NCS,     6,        FifoRead,     7);
ERROR(  NCS,     6,         Destroy,     8);

ERROR(  Mov,     7,   ReadGraphFile,     1);
ERROR(  Mov,     7,     TooFewBytes,     2);

#undef ERROR





int elaborate_packet(RecvBuffer *rbuffer, int rl, SendBuffer *sbuffer, int *sl);
int elaborate_image(byte *imbuffer, int iml, detection2 *dets, int index);
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

	printf("%s\t%dx%d, %s\n", config.hello, config.width, config.height, config.isBMP ? "BMP" : "JPEG");

	buffer_size = (OH_SIZE + config.width*config.height*3) >> (config.isBMP ? 0 : 4);
	image_size = config.width*config.height*3;

	printf("Buffer size set to %d.\n", buffer_size);

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
				int sl;
				if(!elaborate_packet(rbuffer, rl, &sbuffer, &sl)) {
					if(!socket_wait_data(SEND)) {
						sl = send(sockfd_read, &sbuffer, 4 + 4 + 32, 0);
						if(sl <= 0) {
							printf("\n[%s::%d]\t""SO"" ""Send""\t(errno#%d=%s)""(%d)"".\n\n", __FILE__, __LINE__, errno, strerror(errno),sl);
							socket_errno = SOSendError;
						}
						else continue;
					}
				}
			} else if(rl == 0) {
				printf("[Warning %s::%d]: recv return 0.\n", __FILE__, __LINE__);
			} else {
				printf("\n[%s::%d]\t""SO"" ""Send""\t(errno#%d=%s)""(%d)"".\n\n", __FILE__, __LINE__, errno, strerror(errno), rl);
				socket_errno = SOSendError;
			}
		}
		usleep(1000000);
	}
}


int elaborate_packet(RecvBuffer *packet, int rl, SendBuffer *sbuffer, int *sl) {

	int index, ret;

	RIFE(packet->stx != STX, Pck, STX, "(%d != %d).", packet->stx, STX);


	RIFE(packet->l + OH_SIZE > rl, Pck, TooSmall, "(%d > %d).", packet->l + OH_SIZE, rl);

	index = packet->index;
	printf("Size of %d bytes, index=%d\r", packet->l, packet->index);

	sbuffer->stx = STX;
	

	sbuffer->n = elaborate_image(packet->image, packet->l, sbuffer->dets, index);
	if(sbuffer->n < 0) return -1;

	return 0;
}


int elaborate_image(byte *imbuffer, int iml, detection2 *dets, int index) {
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
		sprintf(filename, "./phs/ph_%d.bmp", index);
		FILE *f = fopen(filename, "wb");
		RIFE(!f, Gen, _, "(fopen error)");
		int ret = fwrite(imbuffer, 1, iml, f);
		RIFE(ret < iml, Gen, _, "(fwrite error: %d instead of %d)", ret, iml);
		RIFE(fclose(f), Gen, _, "(fclose error)");
	}
	else {
		unsigned long imjl = 200000;
		byte *imj = tjAlloc((int) imjl);
		tjhandle tjh = tjInitCompress();
		ret = tjCompress2(tjh, imbuffer, config.width, 0, config.height, TJPF_BGR, &imj, &imjl, TJSAMP_444, 100, 0);
		RIFE(ret, Tj, Compress, "(%s)", tjGetErrorStr());
		RIFE(tjDestroy(tjh), Tj, Destroy, "(%s)", tjGetErrorStr());

		char filename[30];
		sprintf(filename, "./phs/ph_%d.jpg", index);
		FILE *f = fopen(filename, "wb");
		RIFE(!f, Gen, _, "(fopen error)");
		ret = fwrite(imj, 1, imjl, f);
		RIFE(ret < imjl, Gen, _, "(fwrite error: %d instead of %lu)", ret, imjl);
		RIFE(fclose(f), Gen, _, "(fclose error)");
	}

	// RIFE(iml != yolo_input_size, Im, WrongSize, "(%d != %d)", iml, yolo_input_size);

#ifndef MOV
	return 0;
#endif
	int i = 0;
	float *_yolo_input = &yolo_input[(yolo_image_h - config.height) >> 1];
	while(i <= (iml-3)) {
		_yolo_input[i] = imbuffer[i] / 255.; ++i;
		_yolo_input[i] = imbuffer[i] / 255.; ++i;
		_yolo_input[i] = imbuffer[i] / 255.; ++i;
	}

	// X[i] = imbuffer[i+2] / 255.; ++i;
	// X[i] = imbuffer[i] / 255.;   ++i;
	// X[i] = imbuffer[i-2] / 255.; ++i;
	ret = mov_inference(dets, thresh);
	for(i = ret; i >= 0; --i) {
		printf("%5d#%d) x=%7.6f | y=%7.6f | w=%7.6f | h=%7.6f\to=%7.6f | p=%7.6f\t\t%s\n",
			index, ret - i,
			dets->bbox.x, dets->bbox.y, dets->bbox.w, dets->bbox.h, dets->objectness, dets->prob, dets->name);
	}
	return ret;
}	


int main( int argc, char** argv )
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if(socket_start_server()) exit(socket_errno);
#ifdef MOV
	if(mov_init()) exit(movidius_errno);
#endif
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

	while(1) {
		if(
			socket_wait_connection() ||
			socket_recv_config()		 ||
			socket_receiving() ) {
			if(close(sockfd_read)) {
				printf("\nError during closing [errno=%d].\n", errno);
				break;
			}
		} else {
			break;
		}
	}
#endif

	if(mov_destroy())   exit(movidius_errno);

	return 0;
}
