

typedef struct {
	unsigned id;
	mfile *gl, *lo;
	unsigned os, es, ds, hold;  
	unsigned vmmode; 
	unsigned quit;
} context;

enum { LOCAL, GLOBAL }; //vmmode
#define MAXCONTEXT 10
#define MAXMFILE 10

typedef struct {
	context ctab[MAXCONTEXT];
	mfile gtab[MAXMFILE];
	mfile ltab[MAXMFILE];
} itp;

void initctxlist(mfile *mem);
void initcontext(context *ctx);
void exitcontext(context *ctx);
mfile *bank(context *ctx, object o);

