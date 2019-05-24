/*
 * socket.cpp
 *
 *  Created on: Mar 7, 2019
 *      Author: developer
 */

#include "socket.h"
#include "holooj.h"
#include "ny2.h"


#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <turbojpeg.h>
#include <unistd.h>


#ifdef OPENCV
#include <cv.h>
#include <highgui.h>
#endif


#define PORT_IN 56789
#define QUEUE_LENGTH 3

#define MOV
#define SHOWIMAGE

#define RECV 1
#define SEND 0

#define RECV_CONTINUE { usleep(1000); continue; }


int socket_wait_data(int is_recv);


int socket_errno;
int sockfd_server;
int sockfd_read;
struct pollfd ufds[1];
struct sockaddr_in server_addr;
struct sockaddr_in holo_addr;
int timeouts = 0;


int get_address(struct in_addr *addr, const char *iface) {

	struct ifaddrs *ifaddr, *ifa;
	REPORT_ERRNO(-1 == getifaddrs(&ifaddr), SOAddr, "");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		int condition = !strcmp(ifa->ifa_name, iface) && ifa->ifa_addr && (ifa->ifa_addr->sa_family == AF_INET);

		addr->s_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
	    if (condition) {
	        break;
	    }
	}
	freeifaddrs(ifaddr);

	return 0;
}

int socket_start_server(const char *iface, unsigned int port) {
	int ret;
	
	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);
	REPORT_ERRNO(0 > sockfd_server, SOCreation, "");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(get_address(&server_addr.sin_addr, iface)) return -1;

	ret = bind(sockfd_server, (struct sockaddr *) &server_addr, sizeof(server_addr));
	REPORT_ERRNO(0 > ret, SOBind, "");

	ret = listen(sockfd_server, 3);
	REPORT_ERRNO(0 > ret, SOListening, "");

	return 0;
}


int socket_wait_connection() {
    int addrlen = sizeof(holo_addr);
	timeouts = 0;
	
	sockfd_read = accept(sockfd_server, (struct sockaddr *)&holo_addr, (socklen_t*)&addrlen);
	REPORT_ERRNO(0 > sockfd_read, SOAccept, "");

	ufds[0].fd = sockfd_read;
	ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band

	return 0;
}


int socket_recv(byte *buf, size_t l, int flags) {
	int is_recv = 1;
	int ret;

	if(socket_wait_data(is_recv)) return -1;
	ret = recv(sockfd_read, buf, l, flags);
	REPORT_ERRNO(ret < 0, SORecv, "");
	return ret;
}

int socket_send(byte *buf, size_t l, int flags) {
	int is_recv = 0;
	int ret;

	if(socket_wait_data(is_recv)) return -1;
	ret = send(sockfd_read, buf, l, flags);
	REPORT_ERRNO(ret < 0, SOSend, "");
	return ret;
}

int socket_wait_data(int is_recv) {
	int rv = poll(ufds, 1, 100);
	REPORT_ERRNO(rv == -1, PollGeneric, "");
	if(rv==0) ++timeouts;
	REPORT_ERRNO(rv == 0,  PollTimeout, "");
	if(is_recv) {
		rv = ufds[0].revents | POLLIN;
		REPORT_ERRNO(!rv,  PollIn, "");
	} else {
		rv = ufds[0].revents | POLLOUT;
		REPORT_ERRNO(!rv,  PollOut, "");
	}
	return !rv;
}

int socket_read_close() {
	if(close(sockfd_read)) {
		printf("\nError during closing read socket [errno=%d].\n", errno);
		return -1;
	}
	return 0;
}

int socket_server_close() {
	if(close(sockfd_server)) {
		printf("\nError during closing server socket [errno=%d].\n", errno);
		return -1;
	}
	return 0;
}

int socket_close() {
	if(socket_read_close()) {
		return -1;
	}
	if(socket_server_close()) {
		return -1;
	}
	return 0;
}