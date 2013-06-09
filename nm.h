
typedef struct tst {
    unsigned val,
             lo,     
             eq,
             hi;
} tst;

void initnames(context *ctx);
object consname(context *ctx, char *s);
object strname(context *ctx, object n);


