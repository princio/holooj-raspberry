

#include <errno.h>     
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <unistd.h>

#include <cv.h>
#include <highgui.h>

typedef unsigned char byte;

typedef struct Config {
	int STX;
	int rows;
	int cols;
	int isBMP;
} Config;

typedef struct ImagePacket
{
    int stx;
    int index;
    int l;
    byte image[5];
} ImagePacket;


Config config;
struct pollfd ufds[1];
int sockfd;
ImagePacket *imgpck;
int buffer_size;

const int STX = 767590;
const int OH_SIZE = 12;


int socket_wait_data(int is_recv) {
	int rv = poll(ufds, 1, 100);
	if(is_recv) {
		rv = ufds[0].revents | POLLIN;
	} else {
		rv = ufds[0].revents | POLLOUT;
	}
	return !rv;
}

int load_image() {
    int lr;
    char fpath[100];
	strcpy(fpath, "/home/developer/dog_resized.jpg");
	printf("\n Elaborating --> %s...\n\n", fpath);

	IplImage *mat = cvLoadImage(fpath, CV_LOAD_IMAGE_COLOR);
	lr = mat->height*mat->width*3;

    config.STX = STX;
	config.rows = mat->height;
	config.cols = mat->width;
	config.isBMP = 1;


    buffer_size = OH_SIZE + lr;
    imgpck = calloc(buffer_size, 1);

    memcpy(imgpck->image, mat->imageData, lr);

    imgpck->stx = STX;
    imgpck->l = lr;
    imgpck->index = 17;


    return 0;

}

int main(int argc, char const *argv[])
{
    
    int ret;
    int sockfd;
    int port = 56789;

    if(argc > 1) {
        port = atoi(argv[1]);
    }

    struct sockaddr_in addr;

    ret = inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if(ret < 0) {
        printf("inet_pton: %s.\n", strerror(errno));
        return -1;
    }

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    { 
        printf("\n Socket creation error: %s.\n", strerror(errno)); 
        return -1; 
    } 
    ufds[0].fd = sockfd;
    ufds[0].events = POLLIN | POLLOUT; // check for normal or out-of-band

    load_image();

    ret = -27;
    printf("\n>insert port number:\t");
    scanf("%d", &port);
    addr.sin_port = htons(port);
    if(ret == -27) {
        ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if(ret < 0) {
            printf("connect: %s.\n", strerror(errno));
            return 1;
        } else {
            printf("connected.");
        }
    }

    char c = 'n';
    printf("config:\n\tSTX=%d, rows=%d, cols=%d, isBMP=%d\n",
        config.STX, config.rows, config.cols, config.isBMP);

    // printf("\n>press to send...");
    // while(c != 'y')
    //     scanf("%s", &c);

    usleep(10000);

    ret = send(sockfd, &config, sizeof(Config), 0);
    if(ret < 0) printf("error sending (%s).", strerror(errno));

    // c = 'n';
    // printf("\n>press to send image...");
    // while(c != 'y')
    //     scanf("%s", &c);

    usleep(10000);

    ret = send(sockfd, imgpck, buffer_size, 0);
    if(ret < 0) printf("error sending (%s).", strerror(errno));





    return 0;
}
