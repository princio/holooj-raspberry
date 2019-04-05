



#include <stdio.h>


int main (int argc, char** argv) {


    char buffer[10] = {8, 1, 0, 1, 1, 0, 0, 0};

    int a = (*(int*) buffer);

    printf("%d\n", a);

}