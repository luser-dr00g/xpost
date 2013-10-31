/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */

#include "xpost_memory.h"  // accesses mfile
#include "xpost_object.h"  // operators are objects
#include "xpost_stack.h"  // uses a stack for argument passing
#include "xpost_interpreter.h"  // works with context struct
#include "xpost_error.h"  // operator functions may throw errors
#include "xpost_string.h"  // uses string function to dump operator name
#include "xpost_garbage.h"  // allocate using gballoc
#include "xpost_name.h"  // operator objects have associated names
#include "xpost_dict.h"  // install operators in systemdict, a dict
#include "xpost_operator.h"  // double-check prototypes


/* convert an integertype object to a realtype object */
static
Xpost_Object promote(Xpost_Object o)
{
    return xpost_cons_real(o.int_.val);
}

/* copied from the header file for reference:
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

/* the number of ops, at any given time. */
static
int noop = 0;

/* allocate the OPTAB structure in VM */
void initoptab (context *ctx)
{
    unsigned ent = mtalloc(ctx->gl, 0, MAXOPS * sizeof(oper), 0);
    mtab *tab = (void *)(ctx->gl);
    assert(ent == OPTAB);
    findtabent(ctx->gl, &tab, &ent);
    tab->tab[ent].sz = 0; // so gc will ignore it
    //printf("ent: %d\nOPTAB: %d\n", ent, (int)OPTAB);
}

/* print a dump of the operator struct given opcode */
void dumpoper(context *ctx,
              int opcode)
{
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    oper op = optab[opcode];
    Xpost_Object_Mark nm = { nametype | XPOST_OBJECT_TAG_DATA_FLAG_BANK, 0, op.name };
    Xpost_Object str = strname(ctx, (Xpost_Object)nm);
    char *s = charstr(ctx, str);
    signat *sig = (void *)(ctx->gl->base + op.sigadr);
    printf("<operator %d %d:%*s %p>",
            opcode,
            str.comp_.sz, str.comp_.sz, s,
            (void *)sig[0].fp );
}

Xpost_Object operfromcode(int opcode)
{
    Xpost_Object op;
    op.mark_.tag = operatortype;
    op.mark_.pad0 = 0;
    op.mark_.padw = opcode;
    return op;
}

/* construct an operator object
   if given a function-pointer, attempts to install a new operator 
   in OPTAB, otherwise just perform a lookup
   if installing a new operator, out and in specify the number of
   output values the function may yield and the number of input
   values whose presence and types should be checked,
   there should follow 'in' number of typenames passed after 'in'.
   */
Xpost_Object consoper(context *ctx,
                char *name,
                /*@null@*/ void (*fp)(),
                int out,
                int in, ...)
{
    Xpost_Object nm;
    Xpost_Object o;
    int opcode;
    int i;
    unsigned si;
    unsigned t;
    unsigned vmmode;
    signat *sp;
    oper *optab;
    oper op;

    //fprintf(stderr, "name: %s\n", name);
    assert(ctx->gl->base);

    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    if (!(in < STACKSEGSZ)) {
        printf("!(in < STACKSEGSZ) in consoper(%s, %d. %d)\n", name, out, in);
        exit(EXIT_FAILURE);
    }
    //assert(in < STACKSEGSZ); // or else opexec can't call it using HOLD

    vmmode=ctx->vmmode;
    ctx->vmmode = GLOBAL;
    nm = consname(ctx, name);
    ctx->vmmode = vmmode;

    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
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

/* copy top n elements to holding stack & pop them */
static
void holdn (context *ctx,
            mfile *mem,
            unsigned stacadr,
            int n)
{
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

/* execute an operator function by opcode
   the opcode is the payload of an operator object
 */
void opexec(context *ctx,
            unsigned opcode)
{
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    oper op = optab[opcode];
    signat *sp = (void *)(ctx->gl->base + op.sigadr);
    int i,j;
    int pass;
    int err = unregistered;
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
            Xpost_Object el = top(ctx->lo, ctx->os, j);
            if (t[j] == anytype) continue;
            if (t[j] == xpost_object_get_type(el)) continue;
            if (t[j] == numbertype
                    && (xpost_object_get_type(el) == integertype
                        || xpost_object_get_type(el) == realtype) ) continue;
            if (t[j] == floattype) {
                if (xpost_object_get_type(el) == integertype) {
                    pot(ctx->lo, ctx->os, j, el = promote(el));
                    continue;
                }
                if (xpost_object_get_type(el) == realtype) continue;
            }
            if (t[j] == proctype
                    && xpost_object_get_type(el) == arraytype
                    && xpost_object_is_exe(el)) continue;
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
    /* If we're executing the context's "currentobject",
       set the number of arguments consumed,
       and set a flag declaring that this has been done.
       This is so onerror() can reset the stack (if possible).
    */
    if (ctx->currentobject.tag == operatortype 
            && ctx->currentobject.mark_.padw == opcode) {
        ctx->currentobject.mark_.pad0 = sp[i].in; 
        ctx->currentobject.tag |= XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD;
    } else {
        /* Not executing current op.
           HOLD may *not* be assumed to contain currentobject's arguments.
           clear the flag.
        */
        ctx->currentobject.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD;
    }

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

#include "xpost_op_stack.h"
#include "xpost_op_string.h"
#include "xpost_op_array.h"
#include "xpost_op_dict.h"
#include "xpost_op_boolean.h"
#include "xpost_op_control.h"
#include "xpost_op_type.h"
#include "xpost_op_token.h"
#include "xpost_op_math.h"
#include "xpost_op_file.h"
#include "xpost_op_save.h"
#include "xpost_op_misc.h"
#include "xpost_op_packedarray.h"
#include "xpost_op_param.h"

/* no-op operator useful as a break target.
   put 'breakhere' in the postscript program,
   run interpreter under gdb,
   gdb> b breakhere
   gdb> run
   will break in the breakhere function (of course),
   which you can follow back to the main loop (gdb> next),
   just as it's about to read the next token.
 */
static
void breakhere(context *ctx)
{
    (void)ctx;
    return;
}

/* create systemdict and call
   all initop?* functions, installing all operators */
void initop(context *ctx)
{
    Xpost_Object op;
    Xpost_Object n;
    Xpost_Object sd;
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

    initoppa(ctx, sd);
    initopparam(ctx, sd);

    //push(ctx->lo, ctx->ds, sd); // push systemdict on dictstack

#ifdef DEBUGOP
    printf("final sd:\n");
    dumpstack(ctx->lo, ctx->ds);
    dumpdic(ctx->gl, sd); fflush(NULL);
#endif
}



