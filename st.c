#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "gc.h"
#include "itp.h"

object consstr(mfile *mem, unsigned sz, /*@NULL@*/ char *ini) {
    unsigned ent;
    object o;
    //ent = mtalloc(mem, 0, (sz/sizeof(int) + 1)*sizeof(int));
    ent = gballoc(mem, (sz/sizeof(int) + 1)*sizeof(int));
    if (ini) put(mem, ent, 0, sz, ini);
    o.tag = stringtype;
    o.comp_.sz = sz;
    o.comp_.ent = ent;
    o.comp_.off = 0;
    return o;
}

object consbst(context *ctx, unsigned sz, /*@NULL@*/ char *ini) {
    object s;
    s = consstr(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz, ini);
    if (ctx->vmmode==GLOBAL)
        s.tag |= FBANK;
    return s;
}

/*@dependent@*/ char *charstr(context *ctx, object S) {
    mfile *f;
    mtab *tab;
    unsigned ent = S.comp_.ent;
    f = bank(ctx, S) /*S.tag&FBANK?ctx->gl:ctx->lo*/;
    findtabent(f, &tab, &ent);
    return (void *)(f->base + tab->tab[ent].adr);
}


void strput(mfile *mem, object s, integer i, integer c) {
    byte b = c;
    put(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
}

void bstput(context *ctx, object s, integer i, integer c) {
    strput(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i, c);
}

integer strget(mfile *mem, object s, integer i) {
    byte b;
    get(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
    return b;
}

integer bstget(context *ctx, object s, integer i) {
    return strget(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i);
}

#ifdef TESTMODULE
#include <stdio.h>

#define CNT_STR(s) sizeof(s), s

mfile mem;

int main(void) {
    object s;
    int i;
    initmem(&mem, "x.mem");
    (void)initmtab(&mem);

    s = consstr(&mem, CNT_STR("This is a string"));
    for (i=0; i < s.comp_.sz; i++) {
        putchar(strget(&mem, s, i));
    }
    putchar('\n');
    return 0;
}

#endif

