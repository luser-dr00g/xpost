/*
   For a commentary on these macros, see my answer to this SO question
http://stackoverflow.com/questions/6635851/real-world-use-of-x-macros/6636596#6636596

    error() is the internal error call.
    If the jump-point is set, it does a longjmp back to mainloop, which then
    calls onerror()

    Having unwound the "C" stack already,
    onerror() goes on to schedule a (PS) call to err.ps/signalerror and the rest
    of the process proceeds at the postscript level.
   */

#define AS_BARE(a) a ,
//#define AS_STR(a) #a , //defined in ob.h

#define ERRORS(_) \
    _(noerror) \
    _(dictfull) \
    _(dictstackoverflow) \
    _(dictstackunderflow) \
    _(execstackoverflow) \
    _(execstackunderflow) /*5*/\
    _(handleerror) \
    _(interrupt) \
    _(invalidaccess) \
    _(invalidexit) \
    _(invalidfileaccess) /*10*/\
    _(invalidfont) \
    _(invalidrestore) \
    _(ioerror) \
    _(limitcheck) \
    _(nocurrentpoint) /*15*/\
    _(rangecheck) \
    _(stackoverflow) \
    _(stackunderflow) \
    _(syntaxerror) \
    _(timeout) /*20*/\
    _(typecheck) \
    _(undefined) \
    _(undefinedfilename) \
    _(undefinedresult) \
    _(unmatchedmark) /*25*/\
    _(unregistered) \
    _(VMerror)
enum err { ERRORS(AS_BARE) };
extern char *errorname[] /*= { ERRORS(AS_STR) }*/;
/* puts(errorname[(enum err)limitcheck]); */

extern volatile char *errormsg;

void error(unsigned err, char *msg);
void onerror(context *ctx, unsigned err);

