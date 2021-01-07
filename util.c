#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

void error(char *message) {
    perror(message);
    exit(1);
}