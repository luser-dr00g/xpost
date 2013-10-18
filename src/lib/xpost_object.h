#ifndef XPOST_OBJECT_H
#define XPOST_OBJECT_H

#include <inttypes.h>

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
    _(invalid) \
    _(null) \
    _(mark) \
    _(integer) \
    _(real) \
    _(array) \
    _(dict) \
    _(file) \
    _(operator) \
    _(save) \
    _(name) \
    _(boolean) \
    _(context) \
    _(extended) \
    _(glob) \
    _(string) \
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
    XPOST_OBJECT_TAG_DATA_TYPEMASK = 0x000F, /**< mask to yield Xpost_Object_Type */
    XPOST_OBJECT_TAG_DATA_FVALID = 0x0010, /**< for 'anytype' operator pattern */
    XPOST_OBJECT_TAG_DATA_FACCESS = 0x0060, /**< 2-bit mask for the ACCESS field */
    XPOST_OBJECT_TAG_DATA_FACCESSO = 5,    /**< bitwise offset of the ACCESS field */
    XPOST_OBJECT_TAG_DATA_FLIT = 0x0080,  /**< literal flag: 0=executable, 1=literal */
    XPOST_OBJECT_TAG_DATA_FBANK = 0x0100, /**< select memory-file for composite-object data: 0=local, 1=global */
    XPOST_OBJECT_TAG_DATA_EXTENDEDINT = 0x0200, /**< extended object was originally integer */
    XPOST_OBJECT_TAG_DATA_EXTENDEDREAL = 0x0400, /**< extended object was originally real */
    XPOST_OBJECT_TAG_DATA_FOPARGSINHOLD = 0x0800, /* for onerror to reset stack */
} Xpost_Object_Tag_Data;

/**
 * @enum Xpost_Object_Tag_Access
 * @brief valid values for the ACCESS bitfield
 */
typedef enum
{
    XPOST_OBJECT_TAG_ACCESS_NONE,         /**< WRITE= no,  READ= no,  EXEC= no   */
    XPOST_OBJECT_TAG_ACCESS_EXECUTE_ONLY, /**< WRITE= no,  READ= no,  EXEC= yes  */
    XPOST_OBJECT_TAG_ACCESS_READ_ONLY,    /**< WRITE= no,  READ= yes, EXEC= yes, files: READ  */
    XPOST_OBJECT_TAG_ACCESS_UNLIMITED,    /**< WRITE= yes, READ= yes, EXEC= yes, files: WRITE */
} Xpost_Object_Tag_Access;


/*
 *
 * Typedefs
 *
 */

#ifdef WANT_LARGE_OBJECT
typedef unsigned char byte;
typedef uint32_t word;      // 2x small size
typedef uint64_t dword;     // 2x small size
typedef uint64_t qword;
typedef int64_t integer;    // 2x small size
typedef double real;        // 2x small size
typedef dword addr;         // 2x small size (via dword)
# define XPOST_FMT_WORD(_)    PRI ## _ ## 32
# define XPOST_FMT_DWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_QWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_INTEGER(_) PRI ## _ ## 64
# define XPOST_FMT_REAL       "%f"
# define XPOST_FMT_ADDR       PRI ## _ ## 64
#else
typedef unsigned char byte; // assumed 8-bit
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;
typedef int32_t integer;
typedef float real;        // assumes IEEE 754
typedef dword addr;
# define XPOST_FMT_WORD(_)    PRI ## _ ## 16
# define XPOST_FMT_DWORD(_)   PRI ## _ ## 32
# define XPOST_FMT_QWORD(_)   PRI ## _ ## 64
# define XPOST_FMT_INTEGER(_) PRI ## _ ## 32
# define XPOST_FMT_REAL       "%f"
# define XPOST_FMT_ADDR       PRI ## _ ## 32
#endif

/*
 *
 * Structs
 *
 */

/**
 * @typedef Xpost_Object_mark_
 * @brief A generic object: 2 words and a double-word.
 *
 * To avoid too many structure, many types use .mark_.padw
 * to hold an unsigned integer (eg. operatortype, nametype, filetype).
 * Of course, if a type needs to use pad0, that's a sign that
 * it needs its own struct.
 */
typedef struct
{
    word tag;
    word pad0;
    dword padw;
} Xpost_Object_mark_;

/**
 * @typedef Xpost_Object_int_
 * @brief The integertype object.
 */
typedef struct
{
    word tag;
    word pad;
    integer val;
} Xpost_Object_int_;

/**
 * @typedef Xpost_Object_real_
 * @brief The realtype object.
 */
typedef struct
{
    word tag;
    word pad;
    real val;
} Xpost_Object_real_;

/**
 * @typedef Xpost_Object_extended_
 * @brief A combined integer-real for use in dictionaries
 *        as number keys.
 */
typedef struct
{
    word tag;
    word sign_exp;
    dword fraction;
} Xpost_Object_extended_;

/**
 * @typedef Xpost_Object_comp_
 * @brief The composite object structure, used for strings, arrays, dicts.
 */
typedef struct
{
    word tag;
    word sz;
    word ent;
    word off;
} Xpost_Object_comp_;

/**
 * @typedef Xpost_Object_save_
 * @brief The savetype object, for both user and on the save stack.
 */
typedef struct
{
    word tag;
    word lev;
    dword stk;
} Xpost_Object_save_;

/**
 * @typedef Xpost_Object_saverec_
 * @brief The saverec_ type overlays an object so that it can be stacked.
 *
 * The saverec_ type is not available as a (Postscript) user type.
 *  saverec_s occupy the "current save stack" referred to by the
 * stk field of a save object.
 */
typedef struct
{
    word tag;
    word pad;
    word src;
    word cpy;
} Xpost_Object_saverec_;

/**
 * @typedef Xpost_Object_glob_
 * @brief The globtype object exists only for passing between
 *        iterations of filenameforall.
 *
 * There are no constructors for this type. It has no use outside
 * the filenameforall looping construct.
 */
typedef struct
{
    word tag;
    word off;
    void *ptr;
} Xpost_Object_glob_;

/*
 *
 * Union
 *
 */

/**
 * @typedef Xpost_Object
 * @brief The top-level object union.
 *
 * The tag word overlays the tag words in each subtype, so it can
 * be used to determine an object's type (using the xpost_object_get_type()
 * function which masks-off any flags in the tag).
 */
typedef union
{
    word tag;

    Xpost_Object_mark_ mark_;
    Xpost_Object_int_ int_;
    Xpost_Object_real_ real_;
    Xpost_Object_extended_ extended_;
    Xpost_Object_comp_ comp_;
    Xpost_Object_save_ save_;
    Xpost_Object_saverec_ saverec_;
    Xpost_Object_glob_ glob_;
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
 * @param b A boolean value.
 * @return A new object.
 *
 * This function constructs a booleantype object with value @p b.
 * It sets the type to booleantype, sets unlimited access, sets the
 * pad to 0, sets the value to @p b. It returns the object as literal.
 */
Xpost_Object xpost_cons_bool (bool b);

/**
 * @brief Construct an integertype object with the given value.
 *
 * @param i An integer value, typically defined as int32_t.
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
 * @param r A real value, typically defined as float.
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
 * @param obj The object.
 * @return The type of the object as an #Xpost_Object_Type.
 *
 * This function returns the type of the object @p obj, that is the tag
 * with flags masked-off : obj.tag & #XPOST_OBJECT_TAG_DATA_TYPEMASK
 */
Xpost_Object_Type xpost_object_get_type (Xpost_Object obj);

/**
 * @brief Determine whether the object is composite or not (ie. simple).
 *
 * @param obj The object.
 * @return 1 if the object is composite, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is one of the composite
 * types (arraytype, stringtype, or dicttype), 0 otherwise.
 */
int xpost_object_is_composite (Xpost_Object obj);


/**
 * @brief Determine whether the object is executable or not.
 *
 * @param obj The object.
 * @return 1 if the object is executable, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is executable, 0
 * otherwise.
 *
 * Masks the #XPOST_OBJECT_TAG_DATA_FLIT with the tag and performs
 * a logical NOT. Ie. executable means NOT having the
 * #XPOST_OBJECT_TAG_DATA_FLIT flag set.
 */
int xpost_object_is_exe (Xpost_Object obj);

/**
 * @brief Determine whether the object is literal or not.
 *
 * @param obj The object.
 * @return 1 if the object is literal, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is literal, 0
 * otherwise.
 *
 * Masks the #XPOST_OBJECT_TAG_DATA_FLIT with the tag and performs
 * a double-NOT to normalize the value to the range [0..1].
 */
int xpost_object_is_lit (Xpost_Object obj);


/**
 * @brief Yield the access-field from the object's tag.
 *
 * @param obj The object.
 * @return The access-field from the object's tag.
 *
 * This function returns the access-field from the tag of @p obj
 * a value from #Xpost_Object_Tag_Access.
 *
 * Mask the #XPOST_OBJECT_TAG_DATA_FACCESS with the tag, and shift
 * the result down by #XPOST_OBJECT_TAG_DATA_FACCESSO to return just
 * the (2-) bit field.
 *
 * A general description of the access flag behavior is at
 * https://groups.google.com/d/topic/comp.lang.postscript/ENxhFBqwgq4/discussion
 */
Xpost_Object_Tag_Access xpost_object_get_access (Xpost_Object obj);

/**
 * @brief Return object with access-field set to access.
 *
 * @param obj The object.
 * @param access New access-field value.
 * @return The modified object.
 *
 * This function sets the access-field in @p obj to @p access.
 * It returns the modified object by clearing the access-field with
 * an inverse mask. OR-in the new access field, shifted up by
 * #XPOST_OBJECT_TAG_DATA_FACCESSO.
 */
Xpost_Object xpost_object_set_access (
        Xpost_Object obj,
        Xpost_Object_Tag_Access access);


/**
 * @brief Determine whether the object is readable or not.
 *
 * @param obj The object.
 * @return 1 if the object is readable, 0 otherwise.
 *
 * This function checks the access permissions of @p obj,
 * specially for filetypes. Regular objects have read access if the
 * value is greater than executeonly.
 *
 * Filetype objects have read access only if the value is equal
 * to readonly.
 */
int xpost_object_is_readable (Xpost_Object obj);

/**
 * @brief Determine whether the object is writable or not.
 *
 * @param obj The object.
 * @return 1 if the object is writeable, 0 otherwise.
 *
 * This function checks the access permissions of @p obj.
 * An object is writeable if its access is equal to unlimited.
 */
int xpost_object_is_writeable (Xpost_Object obj);


/**
 * @brief Convert object to executable.
 *
 * @param obj The object.
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
 * @param obj The object.
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
 * @param obj The object to dump.
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
