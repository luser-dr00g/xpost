
#define AS_BARE(a) a ,
//#define AS_STR(a) #a , //defined in ob.h

#define ERRORS(_) \
    _(noerror) \
    _(dictfull) _(dictstackoverflow) _(dictstackunderflow) \
    _(execstackoverflow) _(execstackunderflow) \
    _(handleerror) \
    _(interrupt) \
    _(invalidaccess) \
    _(invalidexit) \
    _(invalidfileaccess) \
    _(invalidfont) \
    _(invalidrestore) \
    _(ioerror) \
    _(limitcheck) \
    _(nocurrentpoint) \
    _(rangecheck) \
    _(stackoverflow) \
    _(stackunderflow) \
    _(syntaxerror) \
    _(timeout) \
    _(typecheck) \
    _(undefined) \
    _(undefinedfilename) \
    _(undefinedresult) \
    _(unmatchedmark) \
    _(unregistered) \
    _(VMerror)
enum err { ERRORS(AS_BARE) };
extern char *errorname[] /*= { ERRORS(AS_STR) }*/;
/* puts(errorname[(enum err)limitcheck]); */

extern char *errormsg;

void error(unsigned err, char *msg);
void onerror(context *ctx, unsigned err);

