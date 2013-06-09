/* operators
   This module is the operator interface.
   It defines the operator constructor consoper,
   and the operator handler function opexec.
   initoptab is called to initialize the optab structure itself.
   initop is called to populate the optab structure.
   */
 
typedef struct signat {
    void (*fp)();
    int in;
    unsigned t;
    int out;
} signat;

typedef struct oper {
    unsigned name;
    int n;
    unsigned sigadr;
} oper;

enum typepat { anytype = NTYPES /*stringtype + 1*/,
    floattype, numbertype, proctype };

#define MAXOPS 50
#define SDSIZE 10

void initoptab(context *ctx);
void dumpoper(context *ctx, int opcode);

object consoper(context *ctx, char *name, /*@null@*/ void (*fp)(), int out, int in, ...);

void opexec(context *ctx, unsigned opcode);

#define INSTALL \
    n.mark_.tag = nametype, n.mark_.pad0 = 0, \
    n.mark_.padw = optab[op.mark_.padw].name, \
    bdcput(ctx, sd, n, op), \
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB)); // recalc

void initop(context *ctx);

