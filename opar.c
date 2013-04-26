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

void Iarray(context *ctx, object I) {
	push(ctx->lo, ctx->os, consbar(ctx, I.int_.val));
}

void Alength(context *ctx, object A) {
	push(ctx->lo, ctx->os, consint(A.comp_.sz));
}

void a_copy(context *ctx, object S, object D) {
	unsigned i;
	for (i = 0; i < S.comp_.sz; i++)
		barput(ctx, D, i, barget(ctx, S, i));
}

void Acopy(context *ctx, object S, object D) {
	if (D.comp_.sz < S.comp_.sz) error("rangecheck");
	a_copy(ctx, S, D);
	push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

void Aget(context *ctx, object A, object I) {
	push(ctx->lo, ctx->os, barget(ctx, A, I.int_.val));
}

void Aput(context *ctx, object A, object I, object O) {
	barput(ctx, A, I.int_.val, O);
}

void Agetinterval(context *ctx, object A, object I, object L) {
	push(ctx->lo, ctx->os, arrgetinterval(A, I.int_.val, L.int_.val));
}

void Aputinterval(context *ctx, object S, object I, object D) {
	a_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

void Aaload(context *ctx, object A) {
	int i;
	for (i = 0; i < A.comp_.sz; i++)
		push(ctx->lo, ctx->os, barget(ctx, A, i));
	push(ctx->lo, ctx->os, A);
}

void Aastore(context *ctx, object A) {
	int i;
	for (i = A.comp_.sz - 1; i >= 0; i--)
		barput(ctx, A, i, pop(ctx->lo, ctx->os));
	push(ctx->lo, ctx->os, A);
}

void Aforall(context *ctx, object A, object P) {
	switch(A.comp_.sz) {
	default:
		push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
		push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
		push(ctx->lo, ctx->es, cvlit(P));
		push(ctx->lo, ctx->es, arrgetinterval(A, 1, A.comp_.sz - 1));
	case 1:
		push(ctx->lo, ctx->es, P);
		push(ctx->lo, ctx->os, barget(ctx, A, 0));
	case 0:
		break;
	}
}

void initopar(context *ctx, object sd) {
	oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
	object n,op;
	op = consoper(ctx, "array", Iarray, 1, 1,
			integertype); INSTALL;
	op = consoper(ctx, "length", Alength, 1, 1,
			arraytype); INSTALL;
	op = consoper(ctx, "copy", Acopy, 1, 2,
			arraytype, arraytype); INSTALL;
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
	op = consoper(ctx, "forall", Aforall, 0, 2,
			arraytype, proctype); INSTALL;
}

