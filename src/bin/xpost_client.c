/*
   This is a simple example of a client calling xpost as a library
   with a postscript program, desiring the raster data of the
   generated image.

TODO:
    define buffer interchange type
 */

#include <stdlib.h>

#include "xpost.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"

char *prog =
    "%%BoundingBox: 200 300 400 500\n"
    "300 400 100 0 360 arc\n"
    "fill\n"
    "showpage\n";

int main() {
    void *buffer_type_object;
    xpost_init();
    xpost_create("bgr",
            XPOST_OUTPUT_BUFFEROUT,
            &buffer_type_object,
            XPOST_SHOWPAGE_RETURN,
            1);
    xpost_run(XPOST_INPUT_STRING, prog);
    xpost_destroy();
    free(buffer_type_object);
    xpost_quit();
}

