#ifndef XPOST_CONTEXT_H
#define XPOST_CONTEXT_H

/**
 * @brief valid values for Xpost_Context::vmmode
 */
enum { LOCAL, GLOBAL };

/** @struct Xpost_Context
 *
 */
typedef struct
{

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
    } opcode_shortcuts;  /**< opcodes for internal use, to avoid lookups */

    Xpost_Object currentobject;  /**< currently-executing object, for error() */

    /*@dependent@*/
    Xpost_Memory_File *gl; /**< global VM */
    /*@dependent@*/
    Xpost_Memory_File *lo; /**< local VM */

    unsigned id; /**< cid for this context */

    unsigned os, es, ds, hold; /**< stack addresses in local VM */
    unsigned long rand_next; /**< random number seed */
    unsigned vmmode; /**< allocating in GLOBAL or LOCAL */
    unsigned state;  /**< process state: running, blocked, iowait */
    unsigned quit;  /**< if 1 cause mainloop() to return, if 0 keep looping */

    Xpost_Object event_handler;
    Xpost_Object window_device;
	const char *device_str;

} Xpost_Context;

int xpost_context_init_ctxlist(Xpost_Memory_File *mem);
int xpost_context_append_ctxlist(Xpost_Memory_File *mem, unsigned cid);

/**
 * @brief initialize the context structure
 */
int xpost_context_init(Xpost_Context *ctx);

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

#endif
