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

/* the interpreter
       eval actions
       core interpreter loop
       bank utility function for extracting from the context the mfile relevant to an object
   */

struct opcuts {
    int contfilenameforall;
    int cvx;
    int opfor;
    int forall;
    int load;
    int loop;
    int repeat;
    int token;
};

typedef struct {
    unsigned id;
    /*@dependent@*/ Xpost_Memory_File *gl, *lo;
    unsigned os, es, ds, hold;
    unsigned long rand_next;
    unsigned vmmode;
    unsigned state;
    unsigned quit;
    Xpost_Object currentobject;
    struct opcuts opcuts;
} context;

enum { LOCAL, GLOBAL }; /* vmmode */
#define MAXCONTEXT 10
#define MAXMFILE 10

typedef struct {
    context ctab[MAXCONTEXT];
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
void initctxlist(Xpost_Memory_File *mem);
void addtoctxlist(Xpost_Memory_File *mem, unsigned cid);
unsigned initctxid(void);
context *ctxcid(unsigned cid);
void initcontext(context *ctx);
void exitcontext(context *ctx);
/*@dependent@*/
Xpost_Memory_File *bank(context *ctx, Xpost_Object o);

extern int TRACE;

void inititp(itp *itp);
void exititp(itp *itp);

/* 3 simple top-level functions */

int createitp(void);
void runitp(void);
void destroyitp(void);

#endif
