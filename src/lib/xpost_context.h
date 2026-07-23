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

#ifndef XPOST_CONTEXT_H
#define XPOST_CONTEXT_H

/**
 * @file xpost_context.h
 * @brief This file provides the context functions.
 *
 * This header provides the Xpost context functions.
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#define MAXCONTEXT 10

/**
 * @brief valid values for Xpost_Context::vmmode
 */
enum { LOCAL, GLOBAL };

/**
 * @brief valid values for Xpost_Context::state
 */
enum { C_FREE, C_IDLE, C_RUN, C_WAIT, C_IOBLOCK, C_ZOMB };

/** @struct Xpost_Context
 * @brief The context structure for a thread of execution of ps code
 */
struct _Xpost_Context {

    struct
    {
        int contfilenameforall;
        int cvx;
        int opfor;
        int forall;
        int load;
        int loop;
        int repeat;
        int forcont;
        int repeatcont;
        int loopcont;
        int arrayforallcont;
        int stringforallcont;
        int dictforallcont;
        int oppop;
        int opexch;
        int opdup;
        int opindex;
        int opadd;
        int opget;
        int opsub;
        int opmul;
        int opeq;
        int opne;
        int oplt;
        int ople;
        int opgt;
        int opge;
        int opif;
        int opifelse;
        int opdef;
        int opput;
        int optype;
        int oproll;
        int token;
        int transform;
        int itransform;
        int rotate;
        int concatmatrix;
        int wrapdone;
    } opcode_shortcuts;  /**< opcodes for internal use, to avoid lookups */

    Xpost_Object currentobject;  /**< currently-executing object, for error() */

    /* cache of name -> value resolutions against the dict stack,
       invalidated in bulk whenever any binding may have changed */
    unsigned int *namecache_gen;   /**< generation per (name index, bank) */
    Xpost_Object *namecache_val;   /**< cached resolution */
    unsigned int namecache_size;   /**< entries allocated */
    unsigned int namebind_gen;     /**< current binding generation */

    Xpost_Object typenames[XPOST_OBJECT_NTYPES]; /**< executable name per type,
                                                      populated on first use */

    /*@dependent@*/
    Xpost_Memory_File *gl; /**< global VM */
    /*@dependent@*/
    Xpost_Memory_File *lo; /**< local VM */

    unsigned int id; /**< cid for this context */

    unsigned int os, es, ds, hold; /**< stack addresses in local VM */
    unsigned long rand_next; /**< random number seed */
    unsigned int vmmode; /**< allocating in GLOBAL or LOCAL */
    unsigned int state;  /**< process state: running, blocked, iowait */
    unsigned int quit;  /**< if 1 cause mainloop() to return, if 0 keep looping */

    Xpost_Object event_handler;
    Xpost_Object window_device;
    const char *device_str;

    int ignoreinvalidaccess; //briefly allow invalid access to put userdict in systemdict (per PLRM)

    int scanner_defer; /**< the token just scanned is a brace procedure:
                            the interpreter pushes it as data rather than
                            executing it. A binary object sequence also
                            scans to an executable array but executes. */

    size_t (*stdout_fn)(void *, const char *, size_t); /**< divert %stdout text */
    void *stdout_user;
    size_t (*stderr_fn)(void *, const char *, size_t); /**< divert %stderr text */
    void *stderr_user;

    char run_error_name[48];  /**< error that ended the last run ("" if none) */
    char run_error_info[128]; /**< errorinfo detail for the same ("" if none) */
    int run_uncaught;         /**< an error unwound past every stopped context */

    unsigned int es_run_base; /**< exec-stack depth at xpost_run entry;
                                    a completed run is truncated back to
                                    this depth so its scheduling frames
                                    cannot accumulate across jobs */
    int job_snapshots; /**< take VM snapshots around each xpost_run job
                            (restored on the quit path); disable for a
                            persistent context serving many runs, where
                            the per-run snapshots would accumulate save
                            levels and pin every run's garbage */

    int (*xpost_interpreter_cid_init)(unsigned int *cid);
    Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void);
    Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void);
    int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall);
};

int xpost_context_init_ctxlist(Xpost_Memory_File *mem);
int xpost_context_append_ctxlist(Xpost_Memory_File *mem, unsigned cid);

/**
 * @brief initialize the context structure
 */
int xpost_context_init(Xpost_Context *ctx,
                       int (*xpost_interpreter_cid_init)(unsigned int *cid),
                       Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                       int (*xpost_interpreter_get_initializing)(void),
                       void (*xpost_interpreter_set_initializing)(int),
                       Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                       Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                       int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall));

/**
 * @brief destroy the context structure, and all components
 */
void xpost_context_exit(Xpost_Context *ctx);

/**
 * @brief utility function for extracting from the context
 *        the mfile relevant to an object
 */
/*@dependent@*/
Xpost_Memory_File *xpost_context_select_memory(Xpost_Context *ctx, Xpost_Object o);

/**
 * @brief print a dump of the context structure data to stdout
 */
void xpost_context_dump(Xpost_Context *ctx);

/**
 * @brief install a function to be called by eval()
 */
int xpost_context_install_event_handler(Xpost_Context *ctx,
                                        Xpost_Object operator,
                                        Xpost_Object device);

/**
 * @brief fork new process with private global and private local vm (jobserver)
 */
unsigned int xpost_context_fork1(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 int (*xpost_interpreter_get_initializing)(void),
                                 void (*xpost_interpreter_set_initializing)(int),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall));

/**
 * @brief fork new process with shared global vm and private local vm (application)
 */
unsigned int xpost_context_fork2(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 int (*xpost_interpreter_get_initializing)(void),
                                 void (*xpost_interpreter_set_initializing)(int),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall));

/**
 * @brief fork new process with shared global and shared local vm (lightweight process)
 */
unsigned int xpost_context_fork3(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall));

/**
 * @}
 */

#endif
