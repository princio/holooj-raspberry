



#include <stdio.h>
#include "socekt.h"
#include "ncs.h"


int main (int argc, char** argv) {
    char *graph = "./yolov2/original/yolov2-tiny-original.graph";
    char *meta = "./yolov2/original/yolov2-tiny-original.meta";
    char *iface = "wlan0";

	setvbuf(stdout, NULL, _IONBF, 0);

	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("HoloOj for Raspberry.\n\t--graph\t\t\tthe path to the graph.\n\t--iface\t\t\tthe network interface to use.\n\t--help, -h\t\tthis help.\n");
		exit(0);
	}
	char graph[100];

	strcpy(graph, "none");
	strcpy(meta, "none");
	strcpy(iface, "none");
	if(argc >= 3) {
		for(int i = 1; i < argc; i++) {
			if(!strcmp(argv[i], "--iface")) {
				iface = argv[i+1];
				printf("Iface set to: %s\n", iface);
			}
			if(!strcmp(argv[i], "--graph")) {
				graph = argv[i+1];
				printf("Graph set to: %s\n", graph);
			}
			if(!strcmp(argv[i], "--meta")) {
				meta  = argv[i+1];
				printf("Meta set to: %s\n", meta);
			}
		}
	}

    printf("Starting:\niface=\"%s\"\ngraph=\"%s\"\nmeta=\"%s\"", iface, graph, meta);

	if(ny2_init(graph, meta)) exit(1);

    int socket = 1;
    if(socket) {
        socket_run(iface);
    }

	ny2_destroy();

}