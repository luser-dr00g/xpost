#ifndef XPOST_NM_H
#define XPOST_NM_H

/* names
   The name mechanism associates strings with integers,
   using a ternary search tree
   and a stack of string objects
   */

typedef struct tst {
    unsigned val,
             lo,
             eq,
             hi;
} tst;

void dumpnames(context *ctx);
void initnames(context *ctx);
object consname(context *ctx, char *s);
object strname(context *ctx, object n);

#endif
