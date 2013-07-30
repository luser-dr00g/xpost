#include <stdbool.h>
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "v.h"
#include "itp.h"
#include "err.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "nm.h"
#include "op.h"
#include "ops.h"

/* helper function */
void a_copy(context *ctx, object S, object D) {
    unsigned i;
    for (i = 0; i < S.comp_.sz; i++)
        barput(ctx, D, i, barget(ctx, S, i));
}

/* int  array  array
   create array of length int */
void Iarray(context *ctx, object I) {
    push(ctx->lo, ctx->os, consbar(ctx, I.int_.val));
}

/* -  [  mark
   start array construction */
/* [ is defined in systemdict as a marktype object */

/* mark obj0..objN-1  ]  array
   end array construction */
void arrtomark(context *ctx) {
    int i;
    object a, v;
    Zcounttomark(ctx);
    i = pop(ctx->lo, ctx->os).int_.val;
    a = consbar(ctx, i);
    for ( ; i > 0; i--){
        v = pop(ctx->lo, ctx->os);
        barput(ctx, a, i-1, v);
    }
    (void)pop(ctx->lo, ctx->os); // pop mark
    push(ctx->lo, ctx->os, a);
}

/* array  length  int
   number of elements in array */
void Alength(context *ctx, object A) {
    push(ctx->lo, ctx->os, consint(A.comp_.sz));
}

/* array index  get  any
   get array element indexed by index */
void Aget(context *ctx, object A, object I) {
    push(ctx->lo, ctx->os, barget(ctx, A, I.int_.val));
}

/* array index any  put  -
   put any into array at index */
void Aput(context *ctx, object A, object I, object O) {
    barput(ctx, A, I.int_.val, O);
}

/* array index count  getinterval  subarray
   subarray of array starting at index for count elements */
void Agetinterval(context *ctx, object A, object I, object L) {
    push(ctx->lo, ctx->os, arrgetinterval(A, I.int_.val, L.int_.val));
}

/* array1 index array2  putinterval  -
   replace subarray of array1 starting at index by array2 */
void Aputinterval(context *ctx, object S, object I, object D) {
    a_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

/* array  aload  a0..aN-1 array
   push all elements of array on stack */
void Aaload(context *ctx, object A) {
    int i;
    for (i = 0; i < A.comp_.sz; i++)
        push(ctx->lo, ctx->os, barget(ctx, A, i));
    push(ctx->lo, ctx->os, A);
}

/* any0..anyN-1 array  astore  array
   pop elements from stack into array */
void Aastore(context *ctx, object A) {
    int i;
    for (i = A.comp_.sz - 1; i >= 0; i--)
        barput(ctx, A, i, pop(ctx->lo, ctx->os));
    push(ctx->lo, ctx->os, A);
}

/* array1 array2  copy  subarray2
   copy elements of array1 to initial subarray of array2 */
void Acopy(context *ctx, object S, object D) {
    if (D.comp_.sz < S.comp_.sz) error(rangecheck, "Acopy");
    a_copy(ctx, S, D);
    push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

/* array proc  forall  -
   execute proc for each element of array */
void Aforall(context *ctx, object A, object P) {
    if (A.comp_.sz == 0) return;
    push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, cvlit(arrgetinterval(A, 1, A.comp_.sz - 1)));
    if (isx(A)) push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, P);
    push(ctx->lo, ctx->os, barget(ctx, A, 0));
}

void initopar(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;
    op = consoper(ctx, "array", Iarray, 1, 1,
            integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "["), mark);
    op = consoper(ctx, "]", arrtomark, 1, 0); INSTALL;
    op = consoper(ctx, "length", Alength, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "get", Aget, 1, 2,
            arraytype, integertype); INSTALL;
    op = consoper(ctx, "put", Aput, 0, 3,
            arraytype, integertype, integertype); INSTALL;
    op = consoper(ctx, "getinterval", Agetinterval, 1, 3,
            arraytype, integertype, integertype); INSTALL;
    op = consoper(ctx, "putinterval", Aputinterval, 0, 3,
            arraytype, integertype, arraytype); INSTALL;
    op = consoper(ctx, "aload", Aaload, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "astore", Aastore, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "copy", Acopy, 1, 2,
            arraytype, arraytype); INSTALL;
    op = consoper(ctx, "forall", Aforall, 0, 2,
            arraytype, proctype); INSTALL;
}

