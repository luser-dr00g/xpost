/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#ifndef XPOST_OPERATOR_H
#define XPOST_OPERATOR_H

/* operators
   This module is the operator interface.
   It defines the operator constructor xpost_operator_cons,
   and the operator handler function xpost_operator_exec.
   xpost_operator_init_optab is called to initialize the optab structure itself.
   initop is called to populate the optab structure.

   nb. Since xpost_operator_cons does a linear search through the optab,
   an obvious optimisation would be to factor-out calls to
   xpost_operator_cons from main-line code. Pre-initialize an object
   somewhere and re-use it where needed. xpost2 did this with
   a global struct of "opcuts" (operator object shortcuts),
   but here it would need to be "global", either in global-vm
   or in the context struct.
   One goal of the planned "quick-launch" option is to remove
   these lookups from the initialization, too.

   ----
   An idea to speed-up typechecks, without radical change.
   Add a `int (*check)()` function pointer to the signature
   which directly implements the stack-checking performed by
   xpost_operator_exec (but without the nasty loops).
   xpost_operator_exec would then call op.sig[i].check() if not null.

   */

typedef struct Xpost_Signature {
    int (*fp)();
    int in;
    unsigned t;
    int out;
} Xpost_Signature;

typedef struct Xpost_Operator {
    unsigned name;
    int n;
    unsigned sigadr;
} Xpost_Operator;

enum typepat { anytype = XPOST_OBJECT_NTYPES /*stringtype + 1*/,
    floattype, numbertype, proctype };

#define MAXOPS 200
#define SDSIZE 10

int xpost_operator_init_optab(Xpost_Context *ctx);
void xpost_operator_dump(Xpost_Context *ctx, int opcode);
Xpost_Object xpost_operator_cons_opcode(int opcode);

Xpost_Object xpost_operator_cons(Xpost_Context *ctx, char *name, /*@null@*/ int (*fp)(), int out, int in, ...);

int xpost_operator_exec(Xpost_Context *ctx, unsigned opcode);

#define INSTALL \
    xpost_memory_table_get_addr(ctx->gl, \
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr), \
    optab = (void *)(ctx->gl->base + optadr), \
    n.mark_.tag = nametype|XPOST_OBJECT_TAG_DATA_FLAG_BANK, \
    n.mark_.pad0 = 0, \
    n.mark_.padw = optab[op.mark_.padw].name, \
    xpost_dict_put(ctx, sd, n, op), \
    optab = (void *)(ctx->gl->base + optadr); // recalc

#endif
