/* operators
   This module is the operator interface.
   It defines the operator constructor consoper,
   and the operator handler function opexec.
   initoptab is called to initialize the optab structure itself.
   initop is called to populate the optab structure.

   nb. Since consoper does a linear search through the optab,
   an obvious optimisation would be to factor-out calls to 
   consoper from main-line code. Pre-initialize an object
   somewhere and re-use it where needed. xpost2 did this with
   a global struct of "opcuts" (operator object shortcuts),
   but here it would need to be "global", either in global-vm
   or in the context struct.
   One goal of the planned "quick-launch" option is to remove
   these lookups from the initialization, too.
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

