#include <stdio.h>
#include <stdlib.h>

/* placeholder error function */
/* ultimately, this will do a longjmp back to the central loop */
void error(char *msg) {
    fprintf(stderr, "%s\n", msg);
    perror("last system error:");
    exit(EXIT_FAILURE);
}

