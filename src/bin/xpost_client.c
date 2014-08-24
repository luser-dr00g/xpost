/*
   This is a simple example of a client calling xpost as a library
   with a postscript program, desiring the raster data of the 
   generated image.

TODO:
    add to makefile and make compilable
    define buffer interchange type
 */

#include <stdlib.h>
#include "xpost.h"
#include "xpost_interpreter.h"

char *prog =
    "300 400 100 0 360 arc\n"
    "fill\n"
    "showpage\n";

int main() {
    void *buffer_type_object;
    xpost_create("bgr", XPOST_OUTPUT_BUFFEROUT, &buffer_type_object, 1);
    xpost_run(XPOST_INPUT_STRING, prog);
    xpost_destroy();
    free(buffer_type_object);
}

