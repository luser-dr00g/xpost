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
#include "xpost_dev_bgr.h"

char *prog =
    "%%BoundingBox: 200 300 400 500\n"
    "0 0 1 setrgbcolor\n"
    "300 400 100 0 360 arc\n"
    "fill\n"
    "showpage\n";

int main() {
    Xpost_Context *ctx;
    void *buffer_type_object;
    int ret;

    xpost_init();
    if (!(ctx = xpost_create("bgr",
            XPOST_OUTPUT_BUFFEROUT,
            &buffer_type_object,
            XPOST_SHOWPAGE_RETURN,
            1,
            1)))
        fprintf(stderr, "unable to create interpreter context"), exit(0);
    printf("created interpreter context. executing program...\n");
    ret = xpost_run(ctx, XPOST_INPUT_STRING, prog);
    printf("executed program. xpost_run returned %s\n", ret? "yieldtocaller": "zero");
    {
        typedef struct { unsigned char blue, green, red; } pixel;
        pixel *buffer = buffer_type_object;
        int i,j;
        FILE *fp = fopen("xpost_client_out.ppm", "w");
        fprintf(fp, "P3\n612 792\n255\n");
        for (i=0; i<792; i++) {
            for (j=0; j<612; j++) {
                pixel pix = *buffer++;
                fprintf(fp, "%d ", pix.red);
                fprintf(fp, "%d ", pix.green);
                fprintf(fp, "%d ", pix.blue);
                if ((j%20)==0)
                    fprintf(fp, "\n");
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    xpost_destroy(ctx);
    //free(buffer_type_object);
    xpost_quit();
    return 0;
}

