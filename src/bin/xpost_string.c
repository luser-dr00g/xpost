#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include "xpost_memory.h"  // strings live in mfile, accessed via mtab
#include "xpost_object.h"  // strings are objects
#include "xpost_garbage.h"  // strings are allocated using gballoc
#include "xpost_interpreter.h"  // banked strings may live in local or global vm
#include "xpost_string.h"  // double-check prototypes

/* construct a stringtype object
   with optional string value
   */
object consstr(mfile *mem,
               unsigned sz,
               /*@NULL@*/ char *ini)
{
    unsigned ent;
    object o;
    //ent = mtalloc(mem, 0, (sz/sizeof(int) + 1)*sizeof(int), 0);
    ent = gballoc(mem, (sz/sizeof(int) + 1)*sizeof(int), stringtype);
    if (ini) put(mem, ent, 0, sz, ini);
    o.tag = stringtype | (unlimited << FACCESSO);
    o.comp_.sz = sz;
    o.comp_.ent = ent;
    o.comp_.off = 0;
    return o;
}

/* construct a banked string object
   with optional string value
   */
object consbst(context *ctx,
               unsigned sz,
               /*@NULL@*/ char *ini)
{
    object s;
    s = consstr(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz, ini);
    if (ctx->vmmode==GLOBAL)
        s.tag |= FBANK;
    return s;
}

/* adapter:
            char* <- string object
    yield a real, honest-to-goodness pointer to the
    string in a stringtype object
    */
/*@dependent@*/
char *charstr(context *ctx,
              object S)
{
    mfile *f;
    mtab *tab;
    unsigned ent = S.comp_.ent;
    f = bank(ctx, S) /*S.tag&FBANK?ctx->gl:ctx->lo*/;
    findtabent(f, &tab, &ent);
    return (void *)(f->base + tab->tab[ent].adr + S.comp_.off);
}


/* put a value at index into a string */
void strput(mfile *mem,
            object s,
            integer i,
            integer c)
{
    byte b = c;
    put(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
}

/* put a value at index into a banked string */
void bstput(context *ctx,
            object s,
            integer i,
            integer c)
{
    strput(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i, c);
}

/* get a value from a string at index */
integer strget(mfile *mem,
               object s,
               integer i)
{
    byte b;
    get(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
    return b;
}

/* get a value from a banked string at index */
integer bstget(context *ctx,
               object s,
               integer i)
{
    return strget(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i);
}

#ifdef TESTMODULE_ST
#include <stdio.h>

#define CNT_STR(s) sizeof(s), s

mfile mem;

int main (void)
{
    object s;
    int i;
    printf("\n^ st.c\n");
    initmem(&mem, "x.mem");
    (void)initmtab(&mem);

    s = consstr(&mem, CNT_STR("This is a string"));
    for (i=0; i < s.comp_.sz; i++) {
        putchar(strget(&mem, s, i));
    }
    putchar('\n');
    return 0;
}

#endif

