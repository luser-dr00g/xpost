
typedef struct {
	word tag;
	word sz;
	word nused;
	word pad;
} dichead;

int objcmp(context *ctx, object l, object r);
object consdic(mfile *mem, unsigned sz);
object consbdc(context *ctx, unsigned sz);
unsigned diclength(mfile *mem, object d);
unsigned dicmaxlength(mfile *mem, object d);
bool dicfull(mfile *mem, object d);
bool dicknown(context *ctx, mfile *mem, object d, object k);
object dicget(context *ctx, mfile *mem, object d, object k);
object bdcget(context *ctx, object d, object k);
void dicput(context *ctx, mfile *mem, object d, object k, object v);
void bdcput(context *ctx, object d, object k, object v);


