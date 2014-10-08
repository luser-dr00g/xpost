/*
   This is a simple example of a client calling xpost as a library
   with a postscript program, desiring the raster data of the
   generated image.

TODO:
    define buffer interchange type
 */

#include <stdlib.h>
#include <stdio.h>

#include "xpost.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"

char *prog =
    "%%BoundingBox: 200 300 400 500\n"
    "300 400 100 0 360 arc\n"
    "fill\n"
    "fubar\n"
    "showpage\n";

int main() {
    void *buffer_type_object;
    xpost_init();
    if (!xpost_create("bgr",
            XPOST_OUTPUT_BUFFEROUT,
            &buffer_type_object,
            XPOST_SHOWPAGE_RETURN,
            1))
        fprintf(stderr, "unable to create interpreter context"), exit(0);
    xpost_run(XPOST_INPUT_STRING, prog);
    {
        unsigned char *buffer = buffer_type_object;
        int i,j;
        FILE *fp = fopen("xpost_client_out.ppm", "w");
        fprintf(fp, "P3\n612 792\n255\n");
        for (i=0; i<792; i++) {
            for (j=0; j<612; j++) {
                unsigned int red, green, blue;
                blue = *buffer++;
                green = *buffer++;
                red = *buffer++;
                ++buffer;
                fprintf(fp, "%d ", red);
                fprintf(fp, "%d ", green);
                fprintf(fp, "%d ", blue);
                if ((j%20)==0)
                    fprintf(fp, "\n");
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    xpost_destroy();
    free(buffer_type_object);
    xpost_quit();
    return 0;
}

