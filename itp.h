

typedef struct {
	mfile *gl, *lo;
	unsigned os, es, ds, hold;  
	unsigned vmmode; 
	unsigned quit;
} context;

enum { LOCAL, GLOBAL };

void initcontext(context *ctx);

