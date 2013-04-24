#include <stdbool.h> /* ob.h:bool */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"

void Istring(context *ctx, object I) {
	push(ctx->lo, ctx->os, consbst(ctx, I.int_.val, NULL));
}

void Slength(context *ctx, object S) {
	push(ctx->lo, ctx->os, consint(S.comp_.sz));
}

void s_copy(context *ctx, object S, object D) {
	unsigned i;
	for (i = 0; i < S.comp_.sz; i++)
		bstput(ctx, D, i, bstget(ctx, S, i));
}

void Scopy(context *ctx, object S, object D) {
	if (D.comp_.sz < S.comp_.sz) error("rangecheck");
	s_copy(ctx, S, D);
	push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

void Sget(context *ctx, object S, object I) {
	push(ctx->lo, ctx->os, consint(bstget(ctx, S, I.int_.val)));
}

void Sput(context *ctx, object S, object I, object C) {
	bstput(ctx, S, I.int_.val, C.int_.val);
}

void Sgetinterval(context *ctx, object S, object I, object L) {
	push(ctx->lo, ctx->os, arrgetinterval(S, I.int_.val, L.int_.val));
}

void Sputinterval(context *ctx, object D, object I, object S) {
	s_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

void initopst(context *ctx, object sd) {
	oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
	object n,op;
	op = consoper(ctx, "string", Istring, 1, 1, integertype); INSTALL;
	op = consoper(ctx, "length", Slength, 1, 1, stringtype); INSTALL;
	op = consoper(ctx, "copy", Scopy, 1, 2, stringtype, stringtype); INSTALL;
	op = consoper(ctx, "get", Sget, 1, 2, stringtype, integertype); INSTALL;
	op = consoper(ctx, "put", Sput, 0, 3,
			stringtype, integertype, integertype); INSTALL;
	op = consoper(ctx, "getinterval", Sgetinterval, 1, 3,
			stringtype, integertype, integertype); INSTALL;
	op = consoper(ctx, "putinterval", Sputinterval, 0, 3,
			stringtype, integertype, stringtype); INSTALL;
	//bdcput(ctx, sd, consname(ctx, "mark"), mark);
}
