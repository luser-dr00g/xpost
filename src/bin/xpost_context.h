
/**
 * @brief valid values for context::vmmode
 */
enum { LOCAL, GLOBAL };

/** @struct context
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

} context;

void initctxlist(Xpost_Memory_File *mem);
void addtoctxlist(Xpost_Memory_File *mem, unsigned cid);

/**
 * @brief initialize the context structure
 */
void initcontext(context *ctx);

/**
 * @brief destroy the context structure, and all components
 */
void exitcontext(context *ctx);

/**
 * @brief utility function for extracting from the context
 *        the mfile relevant to an object
 */
/*@dependent@*/
Xpost_Memory_File *bank(context *ctx, Xpost_Object o);

/**
 * @brief print a dump of the context structure data to stdout
 */
void dumpctx(context *ctx);

