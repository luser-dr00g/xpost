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

//#define TESTMODULE

#include <stdio.h>

#include "ob.h" // double-check prototypes

void dumpcompobject(object o);

/* printable strings corresponding to enum types */
char *types[] = {
    TYPES(AS_TYPE_STR)
    "invalid"
};

/* is object executable? */
int isx(object o)
{
    return !(o.tag &FLIT);
}

/* is object literal? */
int islit(object o)
{
    return o.tag & FLIT;
}

/* return the ACCESS field for object */
int faccess(object o)
{
    return (o.tag & FACCESS) >> FACCESSO;
}

/* set the ACCESS field for object, returns new object */
object setfaccess(object o,
                  int access)
{
    o.tag &= ~FACCESS;
    o.tag |= (access << FACCESSO);
    return o;
}

/* does the object have read access? */
int isreadable(object o)
{
    if (type(o) == filetype) {
        return faccess(o) == readonly;
    } else {
        return faccess(o) >= readonly;
    }
}

/* does the object have write access? */
int iswriteable(object o)
{
    return faccess(o) == unlimited;
}

/* return the type from the tag with all flags masked-off */
int type(object o)
{
    return o.tag & TYPEMASK;
}

/* convert to executable
   removes the literal flag in the object, returns new object */
object cvx(object o)
{
    o.tag &= ~FLIT;
    return o;
}

/* convert to literal
   sets the literal flag in the object, returns new object */
object cvlit(object o)
{
    o.tag |= FLIT;
    return o;
}

/* null and mark are both global objects */
// null, mark
SINGLETONS(DEFINE_SINGLETON)

/* construct a booleantype object */
object consbool(bool b)
{
    object o;
    o.int_.tag = booleantype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = b;
    return cvlit(o);
}

/* construct an integertype object */
object consint(integer i)
{
    object o;
    o.int_.tag = integertype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = i;
    return cvlit(o);
}

/* construct a realtype object */
object consreal(real r)
{
    object o;
    o.real_.tag = realtype | (unlimited << FACCESSO);
    o.real_.pad = 0;
    o.real_.val = r;
    return cvlit(o);
}

/* print a dump of the fields in a composite object */
void dumpcompobject(object o)
{
    printf(" %c %u %u %u %u>",
            o.comp_.tag&FBANK? 'G': 'L',
            (unsigned int)o.comp_.tag,
            (unsigned int)o.comp_.sz,
            (unsigned int)o.comp_.ent,
            (unsigned int)o.comp_.off);
}

/* print a dump of the object fields and contents */
void dumpobject(object o)
{
    switch(type(o)) {
        default:
        case invalidtype: printf("<invalid object %04x %04x %04x %04x >",
                                  o.comp_.tag, o.comp_.sz, o.comp_.ent, o.comp_.off); break;

        case nulltype: printf("<null>"); break;
        case marktype: printf("<mark>"); break;
        case booleantype: printf("<boolean %s>", o.int_.val?"true":"false"); break;
        case integertype: printf("<integer %d>", (int)o.int_.val); break;
        case realtype: printf("<real %f>", (float)o.real_.val); break;

        case stringtype: printf("<string"); dumpcompobject(o); break;
        case arraytype: printf("<array"); dumpcompobject(o); break;
        case dicttype: printf("<dict"); dumpcompobject(o); break;

        case nametype: printf("<name %c %u %u %u>",
                               o.mark_.tag&FBANK? 'G':'L',
                               (int)o.mark_.tag,
                               (int)o.mark_.pad0,
                               (int)o.mark_.padw); break;
        case operatortype: printf("<operator %u>", (int)o.mark_.padw); break;
        case filetype: printf("<file>"); break;
        case savetype: printf("<save>"); break;
        case contexttype: printf("<context>"); break;
        case extendedtype: printf("<extended-real>"); break;
        case globtype: printf("<glob>"); break;
    }
}

#ifdef TESTMODULE_OB
int main()
{
    printf("\n^test ob module\n");
    object i = consint(5);
    object j = consint(3579);
    object q = consreal(12.0);
    object r = consreal(.0370);
    object b = consbool(true);
    object c = consbool(false);
    dumpobject(null);
    dumpobject(mark);
    dumpobject(i);
    dumpobject(j);
    dumpobject(q);
    dumpobject(r);
    dumpobject(b);
    dumpobject(c);
    puts("");
    return 0;
} 
#endif
