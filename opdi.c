#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "v.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"

void Idict(context *ctx, object I) {
	push(ctx->lo, ctx->os, consbdc(ctx, I.int_.val));
}

void Dlength(context *ctx, object D) {
	push(ctx->lo, ctx->os, consint(diclength(
					bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
					D)));
}

void Dmaxlength(context *ctx, object D) {
	push(ctx->lo, ctx->os, consint(dicmaxlength(
					bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
					D)));
}

void Dbegin(context *ctx, object D) {
	push(ctx->lo, ctx->ds, D);
}

void Zend(context *ctx) {
	pop(ctx->lo, ctx->ds);
}

void Adef(context *ctx, object K, object V) {
	bdcput(ctx, top(ctx->lo, ctx->ds, 0), K, V);
}

void Aload(context *ctx, object K) {
	int i;
	int z = count(ctx->lo, ctx->ds);
	for (i = 0; i < z; i++) {
		object D = top(ctx->lo,ctx->ds,i);
		if (dicknown(ctx, bank(ctx, D), D, K)) {
			push(ctx->lo, ctx->os, bdcget(ctx, D, K));
			return;
		}
	}
	error("undefined");
}

void initopdi(context *ctx, object sd) {
	oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
	object n,op;
	op = consoper(ctx, "dict", Idict, 1, 1,
			integertype); INSTALL;
	op = consoper(ctx, "length", Dlength, 1, 1, 
			dicttype); INSTALL;
	op = consoper(ctx, "maxlength", Dmaxlength, 1, 1,
			dicttype); INSTALL;
	op = consoper(ctx, "begin", Dbegin, 0, 1,
			dicttype); INSTALL;
	op = consoper(ctx, "end", Zend, 0, 0); INSTALL;
	op = consoper(ctx, "def", Adef, 0, 2,
			anytype, anytype); INSTALL;
	op = consoper(ctx, "load", Aload, 1, 1,
			anytype); INSTALL;
}
