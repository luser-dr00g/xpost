#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "v.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "nm.h"
#include "op.h"
#include "ops.h"

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
	(void)pop(ctx->lo, ctx->ds);
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

/* << k_1 v_1 ... k_N v_N  >>  dict
   construct dictionary from pairs on stack
 */
void dictomark(context *ctx) {
	int i;
	object d, k, v;
	Zcounttomark(ctx);
	i = pop(ctx->lo, ctx->os).int_.val;
	d = consbdc(ctx, i);
	for ( ; i > 0; i -= 2){
		v = pop(ctx->lo, ctx->os);
		k = pop(ctx->lo, ctx->os);
		bdcput(ctx, d, k, v);
	}
	(void)pop(ctx->lo, ctx->os); // pop mark
	push(ctx->lo, ctx->os, d);
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
	bdcput(ctx, sd, consname(ctx, "<<"), mark);
	op = consoper(ctx, ">>", dictomark, 1, 0); INSTALL;
}
