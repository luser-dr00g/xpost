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
    /*@dependent@*/ mfile *gl, *lo;
    unsigned os, es, ds, hold;
    unsigned long rand_next;
    unsigned vmmode;
    unsigned state;
    unsigned quit;
    Xpost_Object currentobject;
    struct opcuts opcuts;
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


#include <setjmp.h>
extern itp *itpdata;
extern int initializing;
extern int ignoreinvalidaccess;
extern jmp_buf jbmainloop;
extern bool jbmainloopset;

mfile *nextltab(void);
mfile *nextgtab(void);
void initctxlist(mfile *mem);
void addtoctxlist(mfile *mem, unsigned cid);
unsigned initctxid(void);
context *ctxcid(unsigned cid);
void initcontext(context *ctx);
void exitcontext(context *ctx);
/*@dependent@*/
mfile *bank(context *ctx, Xpost_Object o);

extern int TRACE;

void inititp(itp *itp);
void exititp(itp *itp);

/* 3 simple top-level functions */

void createitp(void);
void runitp(void);
void destroyitp(void);

#endif
