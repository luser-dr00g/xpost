/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

/**
 * @file xpost_object.c
 * @brief Simple object constructors and functions.
*/

/**
 * @cond LOCAL
 */

#ifdef WANT_LARGE_OBJECT
# define XPOST_FMT_WORD(_)    PRI ## _ ## 32
# define XPOST_FMT_DWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_QWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_INTEGER(_) PRI ## _ ## 64
# define XPOST_FMT_REAL       "f"
# define XPOST_FMT_ADDR       PRIx64
#else
# define XPOST_FMT_WORD(_)    PRI ## _ ## 16
# define XPOST_FMT_DWORD(_)   PRI ## _ ## 32
# define XPOST_FMT_QWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_INTEGER(_) PRI ## _ ## 32
# define XPOST_FMT_REAL       "f"
# define XPOST_FMT_ADDR       PRIx32
#endif

/**
 * @endcond
 */

/*
 *
 * Variables
 *
 */

/* null and mark are both global objects */
XPOST_OBJECT_SINGLETONS(XPOST_OBJECT_DEFINE_SINGLETON)

/**
 * @var xpost_object_type_names
 * @brief Printable strings corresponding to #Xpost_Object_Type.
*/
char *xpost_object_type_names[] =
{
    XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE_STR)
    "invalid"
};


/*
   Constructors for simple types
*/

Xpost_Object xpost_cons_bool (bool b)
{
    Xpost_Object obj;

    obj.int_.tag = booleantype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
            << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    obj.int_.pad = 0;
    obj.int_.val = b;

    return xpost_object_cvlit(obj);
}

Xpost_Object xpost_cons_int (integer i)
{
    Xpost_Object obj;

    obj.int_.tag = integertype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
                << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    obj.int_.pad = 0;
    obj.int_.val = i;

    return xpost_object_cvlit(obj);
}

Xpost_Object xpost_cons_real (real r)
{
    Xpost_Object obj;

    obj.real_.tag = realtype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
                << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    obj.real_.pad = 0;
    obj.real_.val = r;

    return xpost_object_cvlit(obj);
}


/*
   Type and Tag Manipulation
*/

Xpost_Object_Type xpost_object_get_type (Xpost_Object obj)
{
    return obj.tag & XPOST_OBJECT_TAG_DATA_TYPE_MASK;
}

int xpost_object_is_composite (Xpost_Object obj)
{
    switch (xpost_object_get_type(obj))
    {
        case stringtype: /*@fallthrough@*/
        case arraytype: /*@fallthrough@*/
        case dicttype:
            return true;
        default: break;
    }
    return false;
}

int xpost_object_is_exe(Xpost_Object obj)
{
    return !(obj.tag & XPOST_OBJECT_TAG_DATA_FLAG_LIT);
}

int xpost_object_is_lit(Xpost_Object obj)
{
    return !!(obj.tag & XPOST_OBJECT_TAG_DATA_FLAG_LIT);
}

Xpost_Object_Tag_Access xpost_object_get_access (Xpost_Object obj)
{
    return (obj.tag & XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK)
        >> XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET;
}

Xpost_Object xpost_object_set_access (Xpost_Object obj, Xpost_Object_Tag_Access access)
{
    obj.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    obj.tag |= (access << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    return obj;
}

int xpost_object_is_readable(Xpost_Object obj)
{
    if (xpost_object_get_type(obj) == filetype)
    {
        return xpost_object_get_access(obj)
            & XPOST_OBJECT_TAG_ACCESS_FILE_READ;
    }
    else
    {
        return xpost_object_get_access(obj)
            >= XPOST_OBJECT_TAG_ACCESS_READ_ONLY;
    }
}

int xpost_object_is_writeable (Xpost_Object obj)
{
    if (xpost_object_get_type(obj) == filetype)
    {
        return xpost_object_get_access(obj)
            & XPOST_OBJECT_TAG_ACCESS_FILE_WRITE;
    }
    else
    {
        return xpost_object_get_access(obj)
            == XPOST_OBJECT_TAG_ACCESS_UNLIMITED;
    }
}

Xpost_Object xpost_object_cvx (Xpost_Object obj)
{
    obj.tag &= ~ XPOST_OBJECT_TAG_DATA_FLAG_LIT;

    return obj;
}

Xpost_Object xpost_object_cvlit (Xpost_Object obj)
{
    obj.tag |= XPOST_OBJECT_TAG_DATA_FLAG_LIT;

    return obj;
}

/**
 * @cond LOCAL
 */

static
void _xpost_object_dump_composite (Xpost_Object obj)
{
    printf(" %c "
            "%" XPOST_FMT_WORD(u) " "
            "%" XPOST_FMT_WORD(u) " "
            "%" XPOST_FMT_WORD(u) " "
            "%" XPOST_FMT_WORD(u) ">",
            obj.comp_.tag & XPOST_OBJECT_TAG_DATA_FLAG_BANK ? 'G' : 'L',
            obj.comp_.tag,
            obj.comp_.sz,
            obj.comp_.ent,
            obj.comp_.off);
}

/**
 * @endcond
 */

void xpost_object_dump (Xpost_Object obj)
{
    switch (xpost_object_get_type(obj))
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
    case integertype: printf("<integer %" XPOST_FMT_INTEGER(d) ">",
                              obj.int_.val);
                      break;
    case realtype: printf("<real %" XPOST_FMT_REAL ">",
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
                           "%" XPOST_FMT_WORD(u) " "
                           "%" XPOST_FMT_WORD(u) " "
                           "%" XPOST_FMT_DWORD(u) ">",
                           obj.mark_.tag & XPOST_OBJECT_TAG_DATA_FLAG_BANK ?
                               'G' : 'L',
                           obj.mark_.tag,
                           obj.mark_.pad0,
                           obj.mark_.padw);
                   break;

    case operatortype: printf("<operator "
                               "%" XPOST_FMT_DWORD(u) ">",
                               obj.mark_.padw);
                       break;
    case filetype: printf("<file "
                           "%" XPOST_FMT_DWORD(u) ">",
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
