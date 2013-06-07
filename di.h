
typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;

int objcmp(context *ctx, object l, object r);
object consdic(/*@dependent@*/ mfile *mem, unsigned sz);
object consbdc(context *ctx, unsigned sz);
unsigned diclength(/*@dependent@*/ mfile *mem, object d);
unsigned dicmaxlength(/*@dependent@*/ mfile *mem, object d);
bool dicfull(/*@dependent@*/ mfile *mem, object d);
void dumpdic(mfile *mem, object d);
bool dicknown(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);
object dicget(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);
object bdcget(context *ctx, object d, object k);
void dicput(context *ctx, /*@dependent@*/ mfile *mem, object d, object k, object v);
void bdcput(context *ctx, object d, object k, object v);


