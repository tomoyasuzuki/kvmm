#include "util.h"
#include <fcntl.h>

int outfd = 0;

void error(char *message) {
    perror(message);
    exit(1);
}

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}