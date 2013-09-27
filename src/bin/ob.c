/*! \file ob.c
   simple object functions
*/
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


#include <stdio.h>

#include "ob.h" // double-check prototypes


/*! \def char *types[]
  printable strings corresponding to enum types
*/

char *types[] = {
    TYPES(AS_TYPE_STR)
    "invalid"
};

/*
   Manipulating the tag field in an object
*/

/*! \fn int isx(object o)
  masks the FLIT (Literal flag) with the tag and performs
  a logical NOT. excutable means NOT having the FLIT flag set.
*/
int isx(object o)
{
    return !(o.tag &FLIT);
}

/*! \fn int islit(object o)
  masks the FLIT (Literal flag) with the tag.
  0 is false, but true is FLIT, not 1.
  Thus all operations using these flag functions
  should work on zero/non-zero, don't assume true is 1.
*/
int islit(object o)
{
    return o.tag & FLIT;
}

/*! \fn int faccess(object o)
   mask the FACCESS (Access Mask) with the tag, and shift the result
   by FACCESSO (Access bit-Offset) to return just the (2-) bit field.
   
   A general description of the access flag behavior is at
https://groups.google.com/d/topic/comp.lang.postscript/ENxhFBqwgq4/discussion
*/
int faccess(object o)
{
    return (o.tag & FACCESS) >> FACCESSO;
}

/*! \fn object setfaccess(object o, int access)
   clear the FACCESS with an inverse mask.
   OR-in the new access field, shifted up by FACCESSO.
*/
object setfaccess(object o,
                  int access)
{
    o.tag &= ~FACCESS;
    o.tag |= (access << FACCESSO);
    return o;
}

/*! \fn int isreadable(object o)
  check the access permissions, specially for filetypes.
  regular objects have read access if the value is greater
  than executeonly.
  filetype objects have read access only if the value is
  equal to readonly.
*/
int isreadable(object o)
{
    if (type(o) == filetype) {
        return faccess(o) == readonly;
    } else {
        return faccess(o) >= readonly;
    }
}

/*! \fn int iswriteable(object o)
  check the access permissions.
  an object is writeable if its access value is equal to unlimited.
*/
int iswriteable(object o)
{
    return faccess(o) == unlimited;
}

/*! \fn int type(object o)
  return the tag ANDed with TYPEMASK which removes the flags.
*/
int type(object o)
{
    return o.tag & TYPEMASK;
}

/*! \fn object cvx(object o)
  removes the FLIT (literal flag) in the object
  with an inverse mask, returns modified object
*/
object cvx(object o)
{
    o.tag &= ~FLIT;
    return o;
}

/* \fn object cvlit(object o)
   convert to literal
   ORs-in the FLIT (literal flag) in the object's tag,
   returns modified object
*/
object cvlit(object o)
{
    o.tag |= FLIT;
    return o;
}


/*
   Constructors for simple types
*/

/* null and mark are both global objects */
// null, mark
SINGLETONS(DEFINE_SINGLETON)

/*! \fn object consbool(bool b)
  set the type to booleantype, and set unlimited access.
  set the pad to 0.
  set the value to the argument b.
  return the object as literal.
 */
object consbool(bool b)
{
    object o;
    o.int_.tag = booleantype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = b;
    return cvlit(o);
}

/*! \fn object consint(integer i)
   set the type to integertype, and set unlimited access.
   set the pad to 0.
   set the value to the argument i.
   return the object as literal.
 */
object consint(integer i)
{
    object o;
    o.int_.tag = integertype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = i;
    return cvlit(o);
}

/*! \fn object consreal(real r)
  set the type to realtype, and set unlimited access.
  set the pad to 0.
  set the value to the argument r.
  return the object as literal.
 */
object consreal(real r)
{
    object o;
    o.real_.tag = realtype | (unlimited << FACCESSO);
    o.real_.pad = 0;
    o.real_.val = r;
    return cvlit(o);
}


/*
   Dump data
*/

/*
   print a dump of the fields in a composite object.
   just the trailing part, common to stringtype, arraytype, dicttype.
 */
static
void dumpcompobject(object o)
{
    printf(" %c %u %u %u %u>",
            o.comp_.tag&FBANK? 'G': 'L',
            (unsigned int)o.comp_.tag,
            (unsigned int)o.comp_.sz,
            (unsigned int)o.comp_.ent,
            (unsigned int)o.comp_.off);
}

/*! \fn void dumpobject(object o)
*/
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
