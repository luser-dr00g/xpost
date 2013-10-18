/** @file xpost_object.c
 *  @brief Simple object constructors and functions.
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

#include "xpost_object.h"

/*
 *
 * Variables
 *
 */

/* null and mark are both global objects */
XPOST_OBJECT_SINGLETONS(XPOST_OBJECT_DEFINE_SINGLETON)

/** @def char *xpost_object_type_names[]
 *  @brief Printable strings corresponding to enum Xpost_type.
*/
char *xpost_object_type_names[] =
{
    XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE_STR)
    "invalid"
};


/* 
   Constructors for simple types
*/

/** @fn object xpost_cons_bool(bool b)
 *  @brief Construct a booleantype object with value b.
 *
 *  Set the type to booleantype, and set unlimited access.
 *  Set the pad to 0.
 *  Set the value to the argument b.
 *  Return the object as literal.
*/
Xpost_Object xpost_cons_bool (bool b)
{
    Xpost_Object obj;

    obj.int_.tag = booleantype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
            << XPOST_OBJECT_TAG_DATA_FACCESSO);
    obj.int_.pad = 0;
    obj.int_.val = b;

    return xpost_object_cvlit(obj);
}


/** @fn object xpost_cons_int(integer i)
 *  @brief Construct an integertype object with value i.
 *
 *  Set the type to integertype, and set unlimited access.
 *  Set the pad to 0.
 *  Set the value to the argument i.
 *  Return the object as literal.
*/
Xpost_Object xpost_cons_int (integer i)
{
    Xpost_Object obj;

    obj.int_.tag = integertype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
                << XPOST_OBJECT_TAG_DATA_FACCESSO);
    obj.int_.pad = 0;
    obj.int_.val = i;

    return xpost_object_cvlit(obj);
}


/** @fn object xpost_cons_real(real r)
 *  @brief Construct a realtype object with value r.
 *
 *  Set the type to realtype, and set unlimited access.
 *  Set the pad to 0.
 *  Set the value to the argument r.
 *  Return the object as literal.
*/
Xpost_Object xpost_cons_real (real r)
{
    Xpost_Object obj;

    obj.real_.tag = realtype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
                << XPOST_OBJECT_TAG_DATA_FACCESSO);
    obj.real_.pad = 0;
    obj.real_.val = r;

    return xpost_object_cvlit(obj);
}


/*
   Type and Tag Manipulation
*/

/** @fn int xpost_object_type(Xpost_Object obj)
 *  @brief Return obj.tag & TYPEMASK to yield an Xpost_type enum value.
*/
int xpost_object_type (Xpost_Object obj)
{
    return obj.tag & XPOST_OBJECT_TAG_DATA_TYPEMASK;
}


/** @fn int xpost_object_is_composite(Xpost_Object obj)
 *  @brief Return 1 if the object is composite, 0 otherwise.
*/
int xpost_object_is_composite (Xpost_Object obj)
{
    switch (xpost_object_type(obj))
    {
        case stringtype: /*@fallthrough@*/
        case arraytype: /*@fallthrough@*/
        case dicttype:
            return true;
    }
    return false;
}


/** @fn int xpost_object_is_exe(Xpost_Object obj)
 *  @brief Return 1 if the object is executable, 0 otherwise.
 *
 *  Masks the FLIT (Literal flag) with the tag and performs
 *  a logical NOT. Ie. executable means NOT having the FLIT flag set.
*/
int xpost_object_is_exe(Xpost_Object obj)
{
    return !(obj.tag & XPOST_OBJECT_TAG_DATA_FLIT);
}


/** @fn int xpost_object_is_lit(Xpost_Object obj)
 *  @brief Return 1 if the object is literal, 0 otherwise.
 *
 *  Masks the FLIT (Literal flag) with the tag and performs
 *  a double-NOT to normalize the value to the range [0..1].
*/
int xpost_object_is_lit(Xpost_Object obj)
{
    return !!(obj.tag & XPOST_OBJECT_TAG_DATA_FLIT);
}


/** @fn xpost_object_get_access (Xpost_Object obj)
 *  @brief Yield the access-field from the object's tag.
 *
 *  Mask the FACCESS (Access Mask) with the tag, and shift the result down
 *  by FACCESSO (Access bit-Offset) to return just the (2-) bit field.
 *
 *  A general description of the access flag behavior is at
 *  https://groups.google.com/d/topic/comp.lang.postscript/ENxhFBqwgq4/discussion
*/
int xpost_object_get_access (Xpost_Object obj)
{
    return (obj.tag & XPOST_OBJECT_TAG_DATA_FACCESS)
        >> XPOST_OBJECT_TAG_DATA_FACCESSO;
}


/** @fn object xpost_object_set_access (Xpost_Object obj, int access)
 *  @brief Return object with access-field set to access.
 *
 *  Clear the access-field with an inverse mask.
 *  OR-in the new access field, shifted up by FACCESSO.
*/
Xpost_Object xpost_object_set_access (Xpost_Object obj, int access)
{
    obj.tag &= ~XPOST_OBJECT_TAG_DATA_FACCESS;
    obj.tag |= (access << XPOST_OBJECT_TAG_DATA_FACCESSO);
    return obj;
}


/*
 *  Check the access permissions of the object, specially for filetypes.
 *  Regular objects have read access if the value is greater than
 *  executeonly.
 *  Filetype objects have read access only if the value is equal
 *  to readonly.
*/
int xpost_object_is_readable(Xpost_Object obj)
{
    if (xpost_object_type(obj) == filetype)
    {
        return xpost_object_get_access(obj)
            == XPOST_OBJECT_TAG_ACCESS_READ_ONLY;
    }
    else
    {
        return xpost_object_get_access(obj)
            >= XPOST_OBJECT_TAG_ACCESS_READ_ONLY;
    }
}


/*
 *  Check the access permissions of the object.
 *  An object is writeable if its access is equal to unlimited.
*/
int xpost_object_is_writeable (Xpost_Object obj)
{
    return xpost_object_get_access(obj)
        == XPOST_OBJECT_TAG_ACCESS_UNLIMITED;
}


/** @fn object xpost_object_cvx(Xpost_Object obj)
 *  @brief Return object, with executable attribute set to executable.
 *
 *  Convert to executable. Removes the FLIT (literal flag)
 *  in the object's tag with an inverse mask. Returns modified object.
*/
Xpost_Object xpost_object_cvx (Xpost_Object obj)
{
    obj.tag &= ~ XPOST_OBJECT_TAG_DATA_FLIT;

    return obj;
}


/** @fn Xpost_Object xpost_object_cvlit(Xpost_Object obj)
 *  @brief Return object, with executable attribute set to literal.
 *
 *  Convert to Literal. OR-in the FLIT (literal flag) in the object's tag.
 *  Returns modified object.
*/
Xpost_Object xpost_object_cvlit (Xpost_Object obj)
{
    obj.tag |= XPOST_OBJECT_TAG_DATA_FLIT;

    return obj;
}

/**
 * @cond LOCAL
 */

static
void _xpost_object_dump_composite (Xpost_Object obj)
{
    printf(" %c "
            XPOST_FMT_WORD(u) " "
            XPOST_FMT_WORD(u) " "
            XPOST_FMT_WORD(u) " "
            XPOST_FMT_WORD(u) ">",
            obj.comp_.tag & XPOST_OBJECT_TAG_DATA_FBANK ? 'G' : 'L',
            obj.comp_.tag,
            obj.comp_.sz,
            obj.comp_.ent,
            obj.comp_.off);
}

/**
 * @endcond
 */

/** @fn void xpost_object_dump(Xpost_Object obj)
 *  @brief print a dump of the object's contents to stdout.
*/
void xpost_object_dump (Xpost_Object obj)
{
    switch (xpost_object_type(obj))
    {
    default: /*@fallthrough@*/
    case invalidtype: printf("<invalid object "
                              "%04" XPOST_FMT_WORD(x) " "
                              "%04" XPOST_FMT_WORD(x) " "
                              "%04" XPOST_FMT_WORD(x) " "
                              "%04" XPOST_FMT_WORD(x) " >",
                              obj.comp_.tag,
                              obj.comp_.sz,
                              obj.comp_.ent,
                              obj.comp_.off);
                      break;

    case nulltype: printf("<null>");
                   break;
    case marktype: printf("<mark>");
                   break;

    case booleantype: printf("<boolean %s>",
                              obj.int_.val ? "true" : "false");
                      break;
    case integertype: printf("<integer " XPOST_FMT_INTEGER(d) ">",
                              obj.int_.val);
                      break;
    case realtype: printf("<real " XPOST_FMT_REAL ">",
                           obj.real_.val);
                   break;

    case stringtype: printf("<string");
                     _xpost_object_dump_composite(obj);
                     break;
    case arraytype: printf("<array");
                    _xpost_object_dump_composite(obj);
                    break;
    case dicttype: printf("<dict");
                   _xpost_object_dump_composite(obj);
                   break;

    case nametype: printf("<name %c "
                           XPOST_FMT_WORD(u) " "
                           XPOST_FMT_WORD(u) " "
                           XPOST_FMT_DWORD(u) ">",
                           obj.mark_.tag & XPOST_OBJECT_TAG_DATA_FBANK ?
                               'G' : 'L',
                           obj.mark_.tag,
                           obj.mark_.pad0,
                           obj.mark_.padw);
                   break;

    case operatortype: printf("<operator "
                               XPOST_FMT_DWORD(u) ">",
                               obj.mark_.padw);
                       break;
    case filetype: printf("<file "
                           XPOST_FMT_DWORD(u) ">",
                           obj.mark_.padw);
                   break;

    case savetype: printf("<save>");
                   break;
    case contexttype: printf("<context>");
                      break;
    case extendedtype: printf("<extended>");
                       break;
    case globtype: printf("<glob>");
                   break;
    }
}


