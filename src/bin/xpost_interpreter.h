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

#ifndef XPOST_ITP_H
#define XPOST_ITP_H

/**
 * @brief the interpreter

 * The interpreter module manages the itpdata structure, allocating
 * contexts from a table, and allocating memory files to the contexts
 * also from tables. The itpdata structure thus encapsulates the entire
 * dynamic state of the interpreter as a whole.
 *
 * The interpreter module also contains functions for eval actions,
 * the core interpreter loop,
   */

#define MAXCONTEXT 10
#define MAXMFILE 10

typedef struct {
    Xpost_Context ctab[MAXCONTEXT];
    unsigned cid;
    Xpost_Memory_File gtab[MAXMFILE];
    Xpost_Memory_File ltab[MAXMFILE];
} itp;


#include <setjmp.h>
extern itp *itpdata;
extern int initializing;
extern int ignoreinvalidaccess;
extern jmp_buf jbmainloop;
extern int jbmainloopset;

Xpost_Memory_File *nextltab(void);
Xpost_Memory_File *nextgtab(void);
unsigned initctxid(void);
Xpost_Context *ctxcid(unsigned cid);

extern int TRACE;

void inititp(itp *itp);
void exititp(itp *itp);

/* 3 simple top-level functions */

int createitp(void);
void runitp(void);
void destroyitp(void);

#endif
