
object consarr(mfile *mem, unsigned sz);
object consbar(context *ctx, unsigned sz);
void arrput(mfile *mem, object a, integer i, object o);
void barput(context *ctx, object a, integer i, object o);
object arrget(mfile *mem, object a, integer i);
object barget(context *ctx, object a, integer i);
object arrgetinterval(object a, integer s, integer n);


