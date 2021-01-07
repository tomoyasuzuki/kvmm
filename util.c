#include "util.h"

void error(char *message) {
    perror(message);
    exit(1);
}