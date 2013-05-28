

typedef struct {
	unsigned id;
	/*@dependent@*/ mfile *gl, *lo;
	unsigned os, es, ds, hold;  
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

void initctxlist(mfile *mem);
context *ctxcid(unsigned cid);
void initcontext(context *ctx);
void exitcontext(context *ctx);
/*@dependent@*/
mfile *bank(context *ctx, object o);

void inititp(itp *itp);
void exititp(itp *itp);
