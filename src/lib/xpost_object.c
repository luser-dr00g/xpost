/** @file xpost_object.c
 *  @brief Simple object constructors and functions.
*/
#ifdef HAVE_CONFIG.H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL.H
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
object xpost_cons_bool (bool b)
{
    object obj;

    obj.int_.tag = booleantype | (unlimited << FACCESSO);
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
object xpost_cons_int (integer i)
{
    object obj;

    obj.int_.tag = integertype | (unlimited << FACCESSO);
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
object xpost_cons_real (real r)
{
    object obj;

    obj.real_.tag = realtype | (unlimited << FACCESSO);
    obj.real_.pad = 0;
    obj.real_.val = r;

    return xpost_object_cvlit(obj);
}


/*
   Type and Tag Manipulation
*/

/** @fn int xpost_object_type(object obj)
 *  @brief Return obj.tag & TYPEMASK to yield an Xpost_type enum value.
*/
int xpost_object_type (object obj)
{
    return obj.tag & XPOST_OBJECT_TYPEMASK;
}


/** @fn int xpost_object_is_composite(object obj)
 *  @brief Return 1 if the object is composite, 0 otherwise.
*/
int xpost_object_is_composite (object obj)
{
    switch (type(obj))
    {
        case stringtype: /*@fallthrough@*/
        case arraytype: /*@fallthrough@*/
        case dicttype:
            return true;
    }
    return false;
}


/** @fn int xpost_object_is_exe(object obj)
 *  @brief Return 1 if the object is executable, 0 otherwise.
 *
 *  Masks the FLIT (Literal flag) with the tag and performs
 *  a logical NOT. Ie. executable means NOT having the FLIT flag set.
*/
int xpost_object_is_exe(object obj)
{
    return !(obj.tag & XPOST_OBJECT_FLIT);
}


/** @fn int xpost_object_is_lit(object obj)
 *  @brief Return 1 if the object is literal, 0 otherwise.
 *
 *  Masks the FLIT (Literal flag) with the tag and performs
 *  a double-NOT to normalize the value to the range [0..1].
*/
int xpost_object_is_lit(object obj)
{
    return !!(obj.tag & XPOST_OBJECT_FLIT);
}


/** @fn xpost_object_get_access (object obj)
 *  @brief Yield the access-field from the object's tag.
 *
 *  Mask the FACCESS (Access Mask) with the tag, and shift the result down
 *  by FACCESSO (Access bit-Offset) to return just the (2-) bit field.
 *
 *  A general description of the access flag behavior is at
 *  https://groups.google.com/d/topic/comp.lang.postscript/ENxhFBqwgq4/discussion
*/
int xpost_object_get_access (object obj)
{
    return (obj.tag & XPOST_OBJECT_FACCESS) >> XPOST_OBJECT_FACCESSO;
}


/** @fn object xpost_object_set_access (object obj, int access)
 *  @brief Return object with access-field set to access.
 *
 *  Clear the access-field with an inverse mask.
 *  OR-in the new access field, shifted up by FACCESSO.
*/
object xpost_object_set_access (object obj, int access)
{
    obj.tag &= ~XPOST_OBJECT_FACCESS;
    obj.tag |= (access << XPOST_OBJECT_FACCESSO);
    return obj;
}


/*
 *  Check the access permissions of the object, specially for filetypes.
 *  Regular objects have read access if the value is greater than
 *  executeonly.
 *  Filetype objects have read access only if the value is equal
 *  to readonly.
*/
int xpost_object_is_readable(object obj)
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
int xpost_object_is_writeable (object obj)
{
    return xpost_object_get_access(obj)
        == XPOST_OBJECT_TAG_ACCESS_UNLIMITED;
}


/** @fn object xpost_object_cvx(object obj)
 *  @brief Return object, with executable attribute set to executable.
 *
 *  Convert to executable. Removes the FLIT (literal flag)
 *  in the object's tag with an inverse mask. Returns modified object.
*/
object xpost_object_cvx (object obj)
{
    obj.tag &= ~FLIT;

    return obj;
}


/** @fn object xpost_object_cvlit(object obj)
 *  @brief Return object, with executable attribute set to literal.
 *
 *  Convert to Literal. OR-in the FLIT (literal flag) in the object's tag.
 *  Returns modified object.
*/
object xpost_object_cvlit (object obj)
{
    obj.tag |= FLIT;

    return obj;
}

/**
 * @cond LOCAL
 */

static
void _xpost_object_dump_composite (object obj)
{
}

/**
 * @endcond
 */

/** @fn void xpost_object_dump(object obj)
 *  @brief print a dump of the object's contents to stdout.
*/
void xpost_object_dump (object obj)
{
}


