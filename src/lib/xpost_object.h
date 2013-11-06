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

#ifndef XPOST_OBJECT_H
#define XPOST_OBJECT_H


/**
 * @file xpost_object.h
 * @brief The file defines the basic object structure, typically 8-bytes.
 * @defgroup xpost_object Object structure
 *
 * @{
 */

/*
 *
 * Macros
 *
 */

/**
 * @def XPOST_OBJECT_TYPES
 * @brief X-macro for defining enum of typenames and
 *        associated string-table.
 */

#define XPOST_OBJECT_TYPES(_) \
    _(invalid)   /*0*/ \
    _(null)      /*1*/ \
    _(mark)      /*2*/ \
    _(integer)   /*3*/ \
    _(real)      /*4*/ \
    _(array)     /*5*/ \
    _(dict)      /*6*/ \
    _(file)      /*7*/ \
    _(operator)  /*8*/ \
    _(save)      /*9*/ \
    _(name)     /*10*/ \
    _(boolean)  /*11*/ \
    _(context)  /*12*/ \
    _(extended) /*13*/ \
    _(glob)     /*14*/ \
    _(magic)    /*15*/ \
    _(string)   /*16*/ \
/* #def XPOST_OBJECT_TYPES */

#define XPOST_OBJECT_AS_TYPE(_) \
    _ ## type ,

#define XPOST_OBJECT_AS_STR(_) \
    #_ ,

#define XPOST_OBJECT_AS_TYPE_STR(_) \
    #_ "type" ,

#define XPOST_OBJECT_DECLARE_SINGLETON(_) \
    extern Xpost_Object _ ;

#define XPOST_OBJECT_DEFINE_SINGLETON(_) \
    Xpost_Object _ = \
    { \
        XPOST_OBJECT_AS_TYPE(_) \
    };

#define XPOST_OBJECT_SINGLETONS(_) \
    _(invalid) \
    _(null) \
    _(mark) \
/* #def XPOST_OBJECT_SINGLETONS */


/*
 *
 * Enums
 *
 */

/**
 * @enum Xpost_Object_Type
 * @brief A value to track the type of object,
 *        and select the correct union member for manipulation.
 */
typedef enum {
    XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE)
    XPOST_OBJECT_NTYPES
} Xpost_Object_Type;

/**
 * @enum Xpost_Object_Tag_Data
 * @brief Bitmasks and bitshift-positions for the flags in the tag.
 */
typedef enum
{
    XPOST_OBJECT_TAG_DATA_TYPE_MASK          = 0x001F, /**< mask to yield Xpost_Object_Type */
    XPOST_OBJECT_TAG_DATA_FLAG_VALID_OFFSET = 5, /* first flag, bit offset to a point above the type mask */
    XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET,    /**< bitwise offset of the ACCESS field */
    XPOST_OBJECT_TAG_DATA_FLAG_LIT_OFFSET =
        XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET + 2, /* access is a 2-bit field, lit must make room */
    XPOST_OBJECT_TAG_DATA_FLAG_BANK_OFFSET,
    XPOST_OBJECT_TAG_DATA_EXTENDED_INT_OFFSET,
    XPOST_OBJECT_TAG_DATA_EXTENDED_REAL_OFFSET,
    XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD_OFFSET,
    XPOST_OBJECT_TAG_DATA_NBITS,  /* this MUST be < 16, the size of the tag field */

    XPOST_OBJECT_TAG_DATA_FLAG_VALID =
        01 << XPOST_OBJECT_TAG_DATA_FLAG_VALID_OFFSET,
           /**< for 'anytype' operator pattern */
    XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK =
        03 << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET,
            /**< 2-bit mask for the ACCESS field */
    XPOST_OBJECT_TAG_DATA_FLAG_LIT =
        01 << XPOST_OBJECT_TAG_DATA_FLAG_LIT_OFFSET,
            /**< literal flag: 0=executable, 1=literal */
    XPOST_OBJECT_TAG_DATA_FLAG_BANK =
        01 << XPOST_OBJECT_TAG_DATA_FLAG_BANK_OFFSET,
            /**< select memory-file for composite-object data:
              0=local, 1=global */
    XPOST_OBJECT_TAG_DATA_EXTENDED_INT =
        01 << XPOST_OBJECT_TAG_DATA_EXTENDED_INT_OFFSET,
            /**< extended object was integer */
    XPOST_OBJECT_TAG_DATA_EXTENDED_REAL =
        01 << XPOST_OBJECT_TAG_DATA_EXTENDED_REAL_OFFSET,
            /**< extended object was real */
    XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD =
        01 << XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD_OFFSET
            /* for onerror to reset stack */
} Xpost_Object_Tag_Data;

/**
 * @enum Xpost_Object_Tag_Access
 * @brief valid values for the ACCESS bitfield in the object's tag.
 *
 * Most objects use 4 levels of access, except files which use 2 flags.
 * Files can therefore be READ, WRITE, or READ|WRITE.
 */
typedef enum
{
    XPOST_OBJECT_TAG_ACCESS_NONE,         /**< WRITE= no,  READ= no,  EXEC= no   */
    XPOST_OBJECT_TAG_ACCESS_EXECUTE_ONLY, /**< WRITE= no,  READ= no,  EXEC= yes  */
    XPOST_OBJECT_TAG_ACCESS_READ_ONLY,    /**< WRITE= no,  READ= yes, EXEC= yes, files: READ  */
    XPOST_OBJECT_TAG_ACCESS_UNLIMITED,    /**< WRITE= yes, READ= yes, EXEC= yes, files: WRITE */

    /* these 2 are for filetype objects only: */

    XPOST_OBJECT_TAG_ACCESS_FILE_WRITE = 1 << 0, /**< file is writeable */
    XPOST_OBJECT_TAG_ACCESS_FILE_READ  = 1 << 1 /**< file is readable */
} Xpost_Object_Tag_Access;


/*
 *
 * Typedefs
 *
 */

#ifdef WANT_LARGE_OBJECT
typedef unsigned char byte;
typedef unsigned int word;      /* 2x small size */
# ifdef _WIN32
 typedef unsigned __int64 dword; /* 2x small size */
 typedef __int64 integer;        /* 2x small size */
# else
 typedef unsigned long dword;    /* 2x small size */
 typedef long integer;           /* 2x small size */
# endif
typedef double real;            /* 2x small size */
typedef dword addr;             /* 2x small size (via dword) */
#else
typedef unsigned char byte;  /* assumed 8-bit */
typedef unsigned short word; /* assumed 16-bit */
typedef unsigned int dword;  /* assumed 32-bit */
typedef int integer;         /* assumed 32-bit */
typedef float real;          /* assumed IEEE 754 32-bit floating-point */
typedef dword addr;
#endif

/*
 *
 * Structs
 *
 */

/**
 * @struct Xpost_Object_Mark
 * @brief A generic object: tag word, pad word, and a double-word.
 *
 * To avoid too many structures, many types use .mark_.padw
 * to hold an unsigned integer (eg. operatortype, nametype, filetype).
 * Of course, if a type needs to use pad0, that's a sign that
 * it needs its own struct.
 */
typedef struct
{
    word tag; /**< (marktype, filetype, operatortype or nametype) | flags */
    word pad0; /**< == 0 */
    dword padw; /**< payload: an unsigned integer,
                  0 in a marktype object,
                  used for an ent (memory table index)
                      which addresses the FILE * in a filetype object,
                  used for opcode in an operatortype object,
                  used for name index in a nametype object. */
} Xpost_Object_Mark;

/**
 * @struct Xpost_Object_Int
 * @brief The integertype object.
 */
typedef struct
{
    word tag; /**< integertype | flags */
    word pad; /**< == 0 */
    integer val; /**< payload integer value */
} Xpost_Object_Int;

/**
 * @struct Xpost_Object_Real
 * @brief The realtype object.
 */
typedef struct
{
    word tag; /**< realtype | flags */
    word pad; /**< == 0 */
    real val; /**< payload floating-point value */
} Xpost_Object_Real;

/**
 * @struct Xpost_Object_Extended
 * @brief A combined integer-real for use in dictionaries
 *        as number keys.
 */
typedef struct
{
    word tag; /**< extendedtype |
                ( XPOST_OBJECT_TAG_DATA_EXTENDED_INT
                or XPOST_OBJECT_TAG_DATA_EXTENDED_REAL ) | other flags */
    word sign_exp; /**< sign and exponent from a double */
    dword fraction; /**< truncated fraction from a double */
} Xpost_Object_Extended;

/**
 * @struct Xpost_Object_Comp
 * @brief The composite object structure, used for strings, arrays, dicts.
 */
typedef struct
{
    word tag; /**< (stringtype, arraytype, or dicttype) | flags */
    word sz; /**< number of bytes in string,
                   number of objects in array,
                   number of key-value pairs in dict */
    word ent; /**< entity. Absolute index into Xpost_Memory_Table */
    word off; /**< byte offset in string,
                    object offset in array,
                    index in dict (only during `forall` operator) */
} Xpost_Object_Comp;

/**
 * @struct Xpost_Object_Save
 * @brief The savetype object, for both user and on the save stack.
 */
typedef struct
{
    word tag; /**< savetype */
    word lev; /**< save-level, index into Save stack */
    dword stk; /**< address of Saverec stack */
} Xpost_Object_Save;

/**
 * @struct Xpost_Object_Saverec
 * @brief The saverec type overlays an object so that it can be stacked.
 *
 * The saverec type is not available as a (Postscript) user type.
 *  saverec's occupy the "current save stack" referred to by the
 * stk field of a save object.
 */
typedef struct
{
    word tag; /**< arraytype or dicttype */
    word pad; /**< == 0 */
    word src; /**< entity number of source, the allocation being used */
    word cpy; /**< entity number of copy, the copy to revert to in restore */
} Xpost_Object_Saverec;

/**
 * @struct Xpost_Object_Glob
 * @brief The globtype object exists only for passing between
 *        iterations of filenameforall.
 *
 * The globtype object is not available as a (Postscript) user type.
 * It has no use outside the filenameforall looping construct.
 */
typedef struct
{
    word tag; /**< globtype */
    word off; /**< index into the filename array */
    void *ptr; /**< ptr to the glob_t struct */
} Xpost_Object_Glob;

/**
 * @struct Xpost_Object_Magic
 * @brief The magictype object exist as dictionary values where they
 *        are treated specially by the dicput and dicget functions.
 */
typedef struct
{
    word tag; /**< magictype */
    word pad;
    struct Xpost_Magic_Pair *pair; /**< pointer to struct containing getter/setter function pointers */
} Xpost_Object_Magic;

/*
 *
 * Union
 *
 */

/**
 * @union Xpost_Object
 * @brief The top-level object union.
 *
 * The tag word overlays the tag words in each subtype, so it can
 * be used to determine an object's type (using the xpost_object_get_type()
 * function which masks-off any flags in the tag).
 */
typedef union
{
    word tag;

    Xpost_Object_Mark mark_;
    Xpost_Object_Int int_;
    Xpost_Object_Real real_;
    Xpost_Object_Extended extended_;
    Xpost_Object_Comp comp_;
    Xpost_Object_Save save_;
    Xpost_Object_Saverec saverec_;
    Xpost_Object_Glob glob_;
    Xpost_Object_Magic magic_;
} Xpost_Object;


/*
 *
 * Variables
 *
 */

/**
 * @def XPOST_OBJECT_SINGLETONS
 * @brief Certain simple objects exist as global template variables
 *        rather than do-nothing constructors.
 */
XPOST_OBJECT_SINGLETONS(XPOST_OBJECT_DECLARE_SINGLETON)

/**
 * @var char *xpost_object_type_names[]
 * @brief A table of strings keyed to the types enum.
 */
extern
char *xpost_object_type_names[]
    /*= { XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE_STR) "invalid"}*/ ;

/*
 *
 * Functions
 *
 */


/*
   Constructors for simple types.

   These objects contain their own values and are not tied
   to a specific context.
 */

/**
 * @brief Construct a booleantype object with the given value.
 *
 * @param[in] b A boolean value.
 * @return A new object.
 *
 * This function constructs a booleantype object with value @p b.
 * It sets the type to booleantype, sets unlimited access, sets the
 * pad to 0, sets the value to @p b. It returns the object as literal.
 */
Xpost_Object xpost_cons_bool (int b);

/**
 * @brief Construct an integertype object with the given value.
 *
 * @param[in] i An integer value, typically defined as int32_t.
 * @return A new object.
 *
 * This function constructs an integertype object with value @p i.
 * It sets the type to integertype, sest unlimited access, sets the
 * pad to 0, set the value to @p i. It returns the object as literal.
 */
Xpost_Object xpost_cons_int (integer i);

/**
 * @brief Construct a realtype object with the given value.
 *
 * @param[in] r A real value, typically defined as float.
 * @return A new object.
 *
 * This function constructs a realtype object with value @p r.
 * It sets the type to realtype, sets unlimited access,
 * sets the pad to 0, sets the value to @p r. It returns the object as
 * literal.
 */
Xpost_Object xpost_cons_real (real r);


/*
   Type and Tag Manipulation

   These functions manipulate the information in the Xpost_Object's
   tag field, which contains the type and various flags and bitfields.
 */

/**
 * @brief Return the object's type, it. the tag with flags masked-off.
 *
 * @param[in] obj The object.
 * @return The type of the object as an #Xpost_Object_Type.
 *
 * This function returns the type of the object @p obj, that is the tag
 * with flags masked-off : obj.tag & #XPOST_OBJECT_TAG_DATA_TYPEMASK
 */
Xpost_Object_Type xpost_object_get_type (Xpost_Object obj);

/**
 * @brief Determine whether the object is composite or not (ie. simple).
 *
 * @param[in] obj The object.
 * @return 1 if the object is composite, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is one of the composite
 * types (arraytype, stringtype, or dicttype), 0 otherwise.
 */
int xpost_object_is_composite (Xpost_Object obj);


/**
 * @brief Determine whether the object is executable or not.
 *
 * @param[in] obj The object.
 * @return 1 if the object is executable, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is executable, 0
 * otherwise.
 *
 * Masks the #XPOST_OBJECT_TAG_DATA_FLAG_LIT with the tag and performs
 * a logical NOT. Ie. executable means NOT having the
 * #XPOST_OBJECT_TAG_DATA_FLAG_LIT flag set.
 */
int xpost_object_is_exe (Xpost_Object obj);

/**
 * @brief Determine whether the object is literal or not.
 *
 * @param[in] obj The object.
 * @return 1 if the object is literal, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is literal, 0
 * otherwise.
 *
 * Masks the #XPOST_OBJECT_TAG_DATA_FLAG_LIT with the tag and performs
 * a double-NOT to normalize the value to the range [0..1].
 */
int xpost_object_is_lit (Xpost_Object obj);


/**
 * @brief Yield the access-field from the object's tag.
 *
 * @param[in] obj The object.
 * @return The access-field from the object's tag.
 *
 * This function returns the access-field from the tag of @p obj
 * a value from #Xpost_Object_Tag_Access.
 *
 * Mask the #XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK with the tag, and shift
 * the result down by #XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET to return just
 * the (2-) bit field.
 *
 * A general description of the access flag behavior is at
 * https://groups.google.com/d/topic/comp.lang.postscript/ENxhFBqwgq4/discussion
 */
Xpost_Object_Tag_Access xpost_object_get_access (Xpost_Object obj);

/**
 * @brief Return object with access-field set to access.
 *
 * @param[in] obj The object.
 * @param[in] access New access-field value.
 * @return The modified object.
 *
 * This function sets the access-field in @p obj to @p access.
 * It returns the modified object by clearing the access-field with
 * an inverse mask. OR-in the new access field, shifted up by
 * #XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET.
 */
Xpost_Object xpost_object_set_access (
        Xpost_Object obj,
        Xpost_Object_Tag_Access access);


/**
 * @brief Determine whether the object is readable or not.
 *
 * @param[in] obj The object.
 * @return 1 if the object is readable, 0 otherwise.
 *
 * This function checks the access permissions of @p obj,
 * specially for filetypes. Regular objects have read access if the
 * value is greater than executeonly.
 *
 * Filetype objects use the access field as 2 independent flags.
 * A file is readable if the FILE_READ flag is set.
 */
int xpost_object_is_readable (Xpost_Object obj);

/**
 * @brief Determine whether the object is writable or not.
 *
 * @param[in] obj The object.
 * @return 1 if the object is writeable, 0 otherwise.
 *
 * This function checks the access permissions of @p obj,
 * specially for filetypes. Regular objects have write access if
 * the value is equal to unlimited.
 *
 * Filetype objects use the access field as 2 independent flags.
 * A file is writeable if the FILE_WRITE flag is set.
 */
int xpost_object_is_writeable (Xpost_Object obj);


/**
 * @brief Convert object to executable.
 *
 * @param[in] obj The object.
 * @return A new object with executable attribute set to executable.
 *
 * The name 'cvx' is borrowed from the Postscript language.
 * cvx is the name of the Postscript operator which performs
 * this function.
 */
Xpost_Object xpost_object_cvx (Xpost_Object obj);

/**
 * @brief Convert object to literal.
 *
 * @param[in] obj The object.
 * @return A new object with executable attribute set to literal.
 *
 * The name 'cvlit' is borrowed from the Postscript language.
 * cvlit is the name of the Postscript operator which performs
 * this function.
 */
Xpost_Object xpost_object_cvlit (Xpost_Object obj);


/*
   Debugging dump.

   This function is used in the backup error handler.
 */

/**
 * @brief print a dump of the object contents to stdout
 *
 * @param[in] obj The object to dump.
 *
 * This function can print the raw object's contents,
 * discriminated by type. It can print the values of
 * simple object, but not composites where it can only
 * print the memory-table index (aka 'ent') and offset.
 *
 * This function is used in the backup error handler
 * which is used for errors in initialization or when the
 * installed error handler fails. Since it is part of a
 * larger information dump, there should also be a dump
 * of the memory-file and memory-tables where the ent
 * may be located.
 */
void xpost_object_dump (Xpost_Object obj);

/**
 * @}
 */

#endif
