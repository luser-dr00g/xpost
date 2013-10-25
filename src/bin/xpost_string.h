#ifndef XPOST_ST_H
#define XPOST_ST_H

/* strings
   */

Xpost_Object consstr(mfile *mem, unsigned sz, /*@NULL@*/ char *ini);
Xpost_Object consbst(context *ctx, unsigned sz, /*@NULL@*/ char *ini);
/*@dependent@*/
char *charstr(context *ctx, Xpost_Object S);
void strput(mfile *mem, Xpost_Object s, integer i, integer c);
void bstput(context *ctx, Xpost_Object s, integer i, integer c);
integer strget(mfile *mem, Xpost_Object s, integer i);
integer bstget(context *ctx, Xpost_Object s, integer i);

#endif
