/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

/**
 * @file xpost_operator.h
 * @brief operator functions
 *
 * This module is the operator interface.
 * It defines the operator constructor xpost_operator_cons,
 * and the operator handler function xpost_operator_exec.
 * xpost_operator_init_optab is called to initialize the optab structure itself.
 * xpost_oplib.c:initop is called to populate the optab structure.
 *
 * nb. Since xpost_operator_cons does a linear search through the optab,
 * an obvious optimisation would be to factor-out calls to
 * xpost_operator_cons from main-line code. Pre-initialize an object
 * somewhere and re-use it where needed. xpost2 did this with
 * a global struct of "opcuts" (operator object shortcuts),
 * but here it would need to be "global", either in global-vm
 * or in the context struct.
 * One goal of the planned "quick-launch" option is to remove
 * these lookups from the initialization, too. One requirement
 * for the quick-launch is removing all function-pointers from vm.
 *
 * ----
 * To speed-up typechecks,
 * a `int (*check)()` function pointer is added to the signature
 * which directly implements the stack-checking performed by
 * xpost_operator_exec (but without the nasty loops).
 * xpost_operator_exec would then call op.sig[i].check() if not null.
 *
 * @{
 */

/**
 * @brief a "generic" function pointer for operator functions
 */
typedef int (*Xpost_Op_Func)();

/**
 * @brief operator signature structure
 *
 * A signature contains a stack-pattern, an optional stack-checking
 * function, and an operator function.
 */
typedef struct Xpost_Signature
{
    Xpost_Op_Func fp;  /* function-pointer which implements the operator action */
    int in;       /* number of argument objects */
    unsigned t;   /* memory address of array of ints representing argument types */
    int (*checkstack)(Xpost_Context *ctx);  /* stack-checking function to bypass generic type-check loop */
    int out;      /* number of output objects */
} Xpost_Signature;

/**
 * @brief operator structure
 *
 * An operator structure, which inhabits the operator table,
 * contains a "pointer" to an array of signatures
 * and the length of that array.
 */
typedef struct Xpost_Operator
{
    unsigned name;   /* name-stack index of operator's name */
    int n;           /* number of signatures */
    unsigned sigadr; /* memory address of array of signatures */
} Xpost_Operator;


/**
 * @brief the type pattern enum
 *
 * extend the type enum with "pattern" types
 * anytype matches any object type
 * floattype matches reals and promotes ints to reals
 * numbertype matches reals and ints
 * proctype matches arrays with executable attribute set
 */
enum typepat
{
    anytype = XPOST_OBJECT_NTYPES /* stringtype + 1 */,
    floattype,
    numbertype,
    proctype };

/**
 * @brief constant size of optab structure
 */
#define MAXOPS 250

/**
 * @brief initial size of systemdict (which then grows, automatically)
 */
#define SDSIZE 10

/**
 * @brief allocate the optab structure
 */
int xpost_operator_init_optab(Xpost_Context *ctx);

/**
 * @brief output a text dump of the operator contents
 */
void xpost_operator_dump(Xpost_Context *ctx, int opcode);

/**
 * @brief construct an operator object by opcode
 */
Xpost_Object xpost_operator_cons_opcode(int opcode);

/**
 * @brief construct an operator object by name,
 *        possibly installing a new operator
 */
Xpost_Object xpost_operator_cons(Xpost_Context *ctx,
                                 const char *name,
                                 /*@null@*/ Xpost_Op_Func fp,
                                 int out,
                                 int in,
                                 ...);

/**
 * @brief execute an operator
 */
int xpost_operator_exec(Xpost_Context *ctx,
                        unsigned opcode);

/**
 * @brief helper macro for installing an operator
 *
 * The INSTALL macro
 * 1. refreshes the optab pointer
 * 2. extracts the name index from the operator referred to by object op
 * 3. constructs a name object n
 * 4. defines the name/operator-object pair in systemdict
 * 5. refreshes the optab pointer yet again
 */
#define INSTALL \
    xpost_memory_table_get_addr(ctx->gl, \
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr), \
    optab = (void *)(ctx->gl->base + optadr), \
    n.mark_.tag = nametype|XPOST_OBJECT_TAG_DATA_FLAG_BANK, \
    n.mark_.pad0 = 0, \
    n.mark_.padw = optab[op.mark_.padw].name, \
    xpost_dict_put(ctx, sd, n, op), \
    optab = (void *)(ctx->gl->base + optadr); // recalc

/**
 * @}
 */

#endif
