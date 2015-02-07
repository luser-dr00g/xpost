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
        int token;
        int transform;
        int itransform;
        int rotate;
        int concatmatrix;
    } opcode_shortcuts;  /**< opcodes for internal use, to avoid lookups */

    Xpost_Object currentobject;  /**< currently-executing object, for error() */

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
