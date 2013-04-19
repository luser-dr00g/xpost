
object consstr(mfile *mem, unsigned sz, char *ini);
object consbst(context *ctx, unsigned sz, char *ini);
void strput(mfile *mem, object s, integer i, integer c);
void bstput(context *ctx, object s, integer i, integer c);
integer strget(mfile *mem, object s, integer i);
integer bstget(context *ctx, object s, integer i);


