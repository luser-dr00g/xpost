#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "opar.h"
#include "oppa.h"

void packedarray (context *ctx, object n) {
    int i;
    object a, v;
    a = consbar(ctx, n.int_.val);
    
    for (i=n.int_.val; i > 0; i--) {
        v = pop(ctx->lo, ctx->os);
        barput(ctx, a, i-1, v);
    }
    a = setfaccess(cvlit(a), readonly);
    push(ctx->lo, ctx->os, a);
}

void setpacking (context *ctx, object b) {
    object sd = bot(ctx->lo, ctx->ds, 0);
    bdcput(ctx, sd, consname(ctx, "currentpacking"), b);
}

void initoppa(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "packedarray", packedarray, 1, 1, integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "currentpacking"), consbool(false));
    op = consoper(ctx, "setpacking", setpacking, 0, 1, booleantype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


