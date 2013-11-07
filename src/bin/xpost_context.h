
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
    /*@dependent@*/ Xpost_Memory_File *gl, *lo;
    unsigned os, es, ds, hold;
    unsigned long rand_next;
    unsigned vmmode;
    unsigned state;
    unsigned quit;
    Xpost_Object currentobject;
    struct opcuts opcuts;
} context;

void initcontext(context *ctx);
void exitcontext(context *ctx);
/*@dependent@*/
Xpost_Memory_File *bank(context *ctx, Xpost_Object o);
void dumpctx(context *ctx);

