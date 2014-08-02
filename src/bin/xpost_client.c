/*
   This is a simple example of a client calling xpost as a library
   with a postscript program, desiring the raster data of the 
   generated image.

TODO:
    add to makefile and make compilable
    pass ps program to xpost
    get raster data
 */

#include "xpost.h"
#include "xpost_interpreter.h"

char *prog =
    "300 400 100 0 360 arc\n"
    "fill\n"
    "showpage\n";

int main() {
    xpost_create("bgr", NULL, "/usr/local/bin", 1);
    xpost_run(XPOST_INPUT_STRING, prog);
    xpost_destroy();
}

