#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "st.h"
#include "gc.h"
#include "nm.h"
#include "di.h"
#include "op.h"

object promote(object o) { return consreal(o.int_.val); }

/*
typedef struct signat {
   void (*fp)();
   int in;
   unsigned t;
   int out;
} signat;

typedef struct oper {
    unsigned name;
    int n; // number of sigs
    unsigned sigadr;
} oper;

enum typepat ( anytype = stringtype + 1,
    floattype, numbertype, proctype };

#define MAXOPS 20
*/

int noop = 0;

void initoptab (context *ctx) {
    unsigned ent = mtalloc(ctx->gl, 0, MAXOPS * sizeof(oper));
    mtab *tab = (void *)(ctx->gl);
    assert(ent == OPTAB);
    findtabent(ctx->gl, &tab, &ent);
    tab->tab[ent].sz = 0; // so gc will ignore it
    //printf("ent: %d\nOPTAB: %d\n", ent, (int)OPTAB);
}

void dumpoper(context *ctx, int opcode) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    oper op = optab[opcode];
    mark_ nm = { nametype, 0, op.name };
    object str = strname(ctx, (object)nm);
    char *s = charstr(ctx, str);
    printf("<operator %d %*s>", opcode, str.comp_.sz, s);
}

object consoper(context *ctx, char *name, /*@null@*/ void (*fp)(), int out,
        int in, ...) {
    object nm;
    object o;
    int opcode;
    int i;
    unsigned si;
    unsigned t;
    signat *sp;
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    oper op;

    if (!(in < STACKSEGSZ)) {
        printf("!(in < STACKSEGSZ) in consoper(%s, %d. %d)\n", name, out, in);
        exit(EXIT_FAILURE);
    }
    //assert(in < STACKSEGSZ); // or else opexec can't call it using HOLD

    nm = consname(ctx, name);
    for (opcode = 0; optab[opcode].name != nm.mark_.padw; opcode++) {
        if (opcode == noop) break;
    }

    /* install a new signature (prototype) */
    if (fp) {
        if (opcode == noop) { /* a new operator */
            unsigned adr;
            if (noop == MAXOPS-1) error(unregistered, "optab too small!\n");
            adr = mfalloc(ctx->gl, sizeof(signat));
            optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB)); // recalc
            op.name = nm.mark_.padw;
            op.n = 1;
            op.sigadr = adr;
            optab[opcode] = op;
            ++noop;
            si = 0;
        } else { /* increase sig table by 1 */
            t = mfrealloc(ctx->gl,
                    optab[opcode].sigadr,
                    optab[opcode].n * sizeof(signat),
                    (optab[opcode].n + 1) * sizeof(signat));
            optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB)); // recalc
            optab[opcode].sigadr = t;

            si = optab[opcode].n++; /* index of last sig */
        }

        sp = (void *)(ctx->gl->base + optab[opcode].sigadr);
        sp[si].t = mfalloc(ctx->gl, in);
        optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB)); // recalc
        sp = (void *)(ctx->gl->base + optab[opcode].sigadr); // recalc
        {
            va_list args;
            byte *b = (void *)(ctx->gl->base + sp[si].t);
            va_start(args, in);
            for (i = in-1; i >= 0; i--) {
                b[i] = va_arg(args, int);
            }
            va_end(args);
            sp[si].in = in;
            sp[si].out = out;
            sp[si].fp = fp;
        }
    } else if (opcode == noop) {
        error(unregistered, "operator not found\n");
    }

    o.tag = operatortype;
    o.mark_.padw = opcode;
    return o;
}

qword digest(context *ctx, mfile *mem, unsigned stacadr) {
    qword a = 0;
    int i;
    (void)ctx;
    for (i=0; i < 8; i++) {
        byte t = type(top(mem, stacadr, i));
        if (t == invalidtype) break;
        a |= (qword)t << ((unsigned)i * 8);
    }
    return a;
}

/* copy top n elements to holding stack & pop them */
void holdn (context *ctx, mfile *mem, unsigned stacadr, int n) {
    stack *hold;
    int j;
    assert(n < TABSZ);
    hold = (void *)(ctx->lo->base + ctx->hold /*adrent(ctx->lo, HOLD)*/);
    hold->top = 0; /* clear HOLD */
    for (j=n; j--;) {
    //j = n;
    //while (j) {
        //j--;
        push(ctx->lo, ctx->hold, top(mem, stacadr, j));
    }
    for (j=n; j--;) {
    //j = n;
    //while (j) {
        (void)pop(mem, stacadr);
        //j--;
    }
}

void opexec(context *ctx, unsigned opcode) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    oper op = optab[opcode];
    signat *sp = (void *)(ctx->gl->base + op.sigadr);
    int i,j;
    bool pass;
    int err;
    char *errmsg = "unspecified error";
    stack *hold;
    int ct;

    ct = count(ctx->lo, ctx->os);
    if (op.n == 0) error(unregistered, "opexec");
    for (i=0; i < op.n; i++) { /* try each signature */
        byte *t;
        if (ct < sp[i].in) {
            pass = false;
            err = stackunderflow;
            errmsg = "opexec";
            continue;
        }
        pass = true;
        t = (void *)(ctx->gl->base + sp[i].t);
        for (j=0; j < sp[i].in; j++) {
            object el = top(ctx->lo, ctx->os, j);
            if (t[j] == anytype) continue;
            if (t[j] == type(el)) continue;
            if (t[j] == numbertype
                    && (type(el) == integertype
                        || type(el) == realtype) ) continue;
            if (t[j] == floattype) {
                if (type(el) == integertype) {
                    pot(ctx->lo, ctx->os, j, el = promote(el));
                    continue;
                }
                if (type(el) == realtype) continue;
            }
            if (t[j] == proctype
                    && type(el) == arraytype
                    && isx(el)) continue;
            pass = false;
            err = typecheck;
            errmsg = "opexec";
            break;
        }
        if (pass) goto call;
    }
    error(err, errmsg);
    return;

call:
    holdn(ctx, ctx->lo, ctx->os, sp[i].in);
    hold = (void *)(ctx->lo->base + ctx->hold /*adrent(ctx->lo, HOLD)*/ );

    switch(sp[i].in) {
        case 0: sp[i].fp(ctx); break;
        case 1: sp[i].fp(ctx, hold->data[0]); break;
        case 2: sp[i].fp(ctx, hold->data[0], hold->data[1]); break;
        case 3: sp[i].fp(ctx, hold->data[0], hold->data[1], hold->data[2]); break;
        case 4: sp[i].fp(ctx, hold->data[0], hold->data[1], hold->data[2],
                        hold->data[3]); break;
        case 5: sp[i].fp(ctx, hold->data[0], hold->data[1], hold->data[2],
                        hold->data[3], hold->data[4]); break;
        case 6: sp[i].fp(ctx, hold->data[0], hold->data[1], hold->data[2],
                        hold->data[3], hold->data[4], hold->data[5]); break;
        default: error(unregistered, "opexec");
    }
}

#include "ops.h"
#include "opst.h"
#include "opar.h"
#include "opdi.h"
#include "opb.h"
#include "opc.h"
#include "opt.h"
#include "optok.h"
#include "opm.h"
#include "opf.h"
#include "opv.h"
#include "opx.h"

void breakhere(context *ctx) {
    (void)ctx;
    return;
}

void initop(context *ctx) {
    object op;
    object n;
    object sd;
    mtab *tab;
    unsigned ent;
    oper *optab;

    sd = consbdc(ctx, SDSIZE);
    bdcput(ctx, sd, consname(ctx, "systemdict"), sd);
    push(ctx->lo, ctx->ds, sd);
    tab = NULL;
    ent = sd.comp_.ent;
    findtabent(ctx->gl, &tab, &ent);
    tab->tab[ent].sz = 0; // make systemdict immune to collection

    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
#ifdef DEBUGOP
    dumpdic(ctx->gl, sd); fflush(NULL);
    puts("");
#endif

    initops(ctx, sd);

//#ifdef DEBUGOP
    //printf("\nops:\n"); dumpdic(ctx->gl, sd); fflush(NULL);
//#endif

    op = consoper(ctx, "breakhere", breakhere, 0, 0); INSTALL;

    initopst(ctx, sd);
    //printf("\nopst:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopar(ctx, sd);
    //printf("\nopar:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopdi(ctx, sd);
    //printf("\nopdi:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopb(ctx, sd);
    //printf("\nopb:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopc(ctx, sd);
    //printf("\nopc:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopt(ctx, sd);
    //printf("\nopt:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initoptok(ctx, sd);
    //printf("\noptok:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopm(ctx, sd);
    //printf("\nopm:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopf(ctx, sd);
    //printf("\nopf:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopv(ctx, sd);
    //printf("\nopv:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    initopx(ctx, sd);
    //printf("\nopx:\n"); dumpdic(ctx->gl, sd); fflush(NULL);

    //push(ctx->lo, ctx->ds, sd); // push systemdict on dictstack

#ifdef DEBUGOP
    printf("final sd:\n");
    dumpstack(ctx->lo, ctx->ds);
    dumpdic(ctx->gl, sd); fflush(NULL);
#endif
}



