#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "gc.h"
#include "itp.h"

object consstr(mfile *mem, unsigned sz, char *ini) {
	unsigned ent = mtalloc(mem, 0, (sz/sizeof(int) + 1)*sizeof(int));
	//unsigned ent = gballoc(mem, (sz/sizeof(int) + 1)*sizeof(int));
	if (ini) put(mem, ent, 0, sz, ini);
	object o;
	o.tag = stringtype;
	o.comp_.sz = sz;
	o.comp_.ent = ent;
	o.comp_.off = 0;
	return o;
}

object consbst(context *ctx, unsigned sz, char *ini) {
	object s = consstr(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz, ini);
	if (ctx->vmmode==GLOBAL)
		s.tag |= FBANK;
	return s;
}

void strput(mfile *mem, object s, integer i, integer c) {
	byte b = c;
	put(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
}

void bstput(context *ctx, object s, integer i, integer c) {
	strput(s.tag&FBANK? ctx->gl: ctx->lo, s, i, c);
}

integer strget(mfile *mem, object s, integer i) {
	byte b;
	get(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
	return b;
}

integer bstget(context *ctx, object s, integer i) {
	return strget(s.tag&FBANK? ctx->gl: ctx->lo, s, i);
}

#ifdef TESTMODULE
#include <stdio.h>

#define CNT_STR(s) sizeof(s), s

mfile mem;

int main(void) {
	initmem(&mem);
	(void)initmtab(&mem);

	object s = consstr(&mem, CNT_STR("This is a string"));
	int i;
	for (i=0; i < s.comp_.sz; i++) {
		putchar(strget(&mem, s, i);
	}
	putchar('\n');
	return 0;
}

#endif

