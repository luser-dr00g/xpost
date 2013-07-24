/* the interpreter
       eval actions
       core interpreter loop
       bank utility function for extracting from the context the mfile relevant to an object
   */

typedef struct {
    unsigned id;
    /*@dependent@*/ mfile *gl, *lo;
    unsigned os, es, ds, hold;  
    unsigned long rand_next;
    unsigned vmmode; 
    unsigned state;
    unsigned quit;
} context;

enum { LOCAL, GLOBAL }; //vmmode
#define MAXCONTEXT 10
#define MAXMFILE 10

typedef struct {
    context ctab[MAXCONTEXT];
    unsigned cid;
    mfile gtab[MAXMFILE];
    mfile ltab[MAXMFILE];
} itp;

extern itp *itpdata;

void initctxlist(mfile *mem);
context *ctxcid(unsigned cid);
void initcontext(context *ctx);
void exitcontext(context *ctx);
/*@dependent@*/
mfile *bank(context *ctx, object o);

extern int TRACE;

void inititp(itp *itp);
void exititp(itp *itp);

