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

/** @def XPOST_OBJECT_TYPES
 *  @brief X-macro for defining enum of typenames and 
 *         associated string-table.
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
    extern object _;

#define XPOST_OBJECT_DEFINE_SINGLETON(_) \
    object _ = \
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

typedef enum {
    XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE)
    NTYPES
} Xpost_Object_Type;

/**
 * @enum Xpost_tag_data
 * @brief Bitmasks and bitshift-positions for the flags in the tag.
 */
typedef enum
{
    XPOST_OBJECT_TYPEMASK = 0x000F,
    XPOST_OBJECT_FVALID = 0x0010, /*< for 'anytype' operator pattern */
    XPOST_OBJECT_FACCESS = 0x0060,
    XPOST_OBJECT_FACCESSO = 5,    /*< bitwise offset of the ACCESS field */
    XPOST_OBJECT_FLIT = 0x0080,
    XPOST_OBJECT_FBANK = 0x0100, /* 0=local, 1=global */
    XPOST_OBJECT_EXTENDEDINT = 0x0200,
    XPOST_OBJECT_EXTENDEDREAL = 0x0400,
    XPOST_OBJECT_FOPARGSINHOLD = 0x0800, /* for onerror to reset stack */
} Xpost_Object_Tag_Data;

/** @def enum Xpost_Object_Tag_Access
 *  @brief valid values for the ACCESS bitfield
 */
typedef enum
{
    XPOST_OBJECT_TAG_ACCESS_NONE,
    XPOST_OBJECT_TAG_ACCESS_EXECUTE_ONLY,
    XPOST_OBJECT_TAG_ACCESS_READ_ONLY,
    XPOST_OBJECT_TAG_ACCESS_UNLIMITED,
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

/** @def typdef struct {} mark_
 *  @brief A generic object: 2 unsigned shorts and an unsigned long.
 *
 *  To avoid too many structure, many types use .mark_.padw
 *  to hold an unsigned value (eg. operatortype, nametype, filetype).
 *  Of course, if a type needs to use pad0, that's a sign that
 *  it needs its own struct.
 */
typedef struct
{
    word tag;
    word pad0;
    dword padw;
} mark_;

/** @def typedef struct {} int_
 *  @brief The integertype object.
 */
typedef struct
{
    word tag;
    word pad;
    integer val;
} int_;

/** @def typedef struct {} real_
 *  @brief The realtype object.
 */
typedef struct
{
    word tag;
    word pad;
    real val;
} real_;

/** @def typedef struct {} extended_
 *  @brief A combined integer-real for use in dictionaries
 *         as number keys.
 */
typedef struct
{
    word tag;
    word sign_exp;
    dword fraction;
} extended_;

/** @def typedef struct {} comp_
 *  @brief The composite object structure, used for strings, arrays, dicts.
 */
typedef struct
{
    word tag;
    word sz;
    word ent;
    word off;
} comp_;

/** @def typedef struct {} save_
 *  @brief The savetype object, for both user and on the save stack.
 */
typedef struct
{
    word tag;
    word lev;
    dword stk;
} save_;

/** @def typedef struct {} saverec_
 *  @brief The saverec_ type is not available as a (Postscript) user type.
 *         saverec_s occupy the "current save stack" referred to by the
 *         stk field of a save object.
 *
 *  The saverec_ type overlays an object so that it can be stacked.
 */
typedef struct
{
    word tag;
    word pad;
    word src;
    word cpy;
} saverec_; 

/** @def typedef struct {} glob_
 *  @brief The globtype object exists only for passing between
 *         iterations of filenameforall.
 *
 *  There are no constructors for this type. It has no use outside
 *  the filenameforall looping construct.
 */
typedef struct
{
    word tag;
    word off;
    void *ptr;
} glob_;

/*
 *
 * Union
 *
 */

/** @def typedef union {} object
 *  @brief The top-level object union.
 *
 *  The tag word overlays the tag words in each subtype, so it can
 *  be used to determine an object's type (using the xpost_object_type()
 *  function which masks-off any flags in the tag).
 */
typedef union
{
    word tag;

    mark_ mark_;
    int_ int_;
    real_ real_;
    extended_ extended_;
    comp_ comp_;
    save_ save_;
    saverec_ saverec_;
    glob_ glob_;
} object;


/*
 *
 * Variables
 *
 */

/** @def XPOST_OBJECT_SINGLETONS
 *  @brief Certain simple objects exist as global template variables
 *         rather than do-nothing constructors.
 */
XPOST_OBJECT_SINGLETONS(XPOST_OBJECT_DECLARE_SINGLETON)

/**
 *  @var char *xpost_object_type_names[]
 *  @brief A table of strings keyed to the types enum.
 */
extern
char *xpost_object_type_names[] /*= { XPOST_OBJECT_TYPES(XPOST_OBJECT_AS_TYPE_STR) "invalid"}*/ ;

/*
 *
 * Functions
 *
 */


/*
   Constructors
 */

/** @fn object xpost_cons_bool(bool b)
 *  @brief Construct a booleantype object with value b.
 */
object xpost_cons_bool (bool b);

/** @fn object xpost_cons_int(integer i)
 *  @brief Construct an integertype object with value i.
 */
object xpost_cons_int (integer i);

/** @fn object xpost_cons_real(real r)
 *  @brief Construct a realtype object with value r.
 */
object xpost_cons_real (real r);


/*
   Type and Tag Manipulation
 */

/**
 * @brief Return the object's type, it. the tag with flags masked-off.
 *
 * @param obj The object.
 * @return The type of the object, an enum Xpost_type value.
 *
 * This function returns the type of the object @p obj, that is the tag
 * with flags masked-off.
 */
int xpost_object_type (object obj);

/**
 * @brief Determine whether the object is composite or not (ie. simple).
 *
 * @param obj The object.
 * @return 1 if the object is composite, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is one of the composite
 * types (arraytype, stringtype, or dicttype), 0 otherwise.
 */
int xpost_object_is_composite (object obj);


/**
 * @brief Determine whether the object is executable or not.
 *
 * @param obj The object.
 * @return 1 if the object is executable, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is executable, 0
 * otherwise.
 */
int xpost_object_is_exe (object obj);

/**
 * @brief Determine whether the object is literal or not.
 *
 * @param obj The object.
 * @return 1 if the object is literal, 0 otherwise.
 *
 * This function returns 1 if the object @p obj is literal, 0
 * otherwise.
 */
int xpost_object_is_lit (object obj);


/**
 * @brief Yield the access-field from the object's tag.
 *
 * @param obj The object.
 * @return The access-field from the object's tag.

 * This function returns the access-field from the object's tag,
 * a value from enum Xpost_tag_access_value. 
 */
int xpost_object_get_access (object obj);

/**
 * @brief Return object with access-field set to access.
 *
 * @param obj The object.
 * @return 
 */
object xpost_object_set_access (object obj, int access);


/**
 * @brief Return 1 if the object is readable, 0 otherwise.
 */
int xpost_object_is_readable (object obj);

/**
 * @brief Return 1 if the object is writeable, 0 otherwise.
 */
int xpost_object_is_writeable (object obj);


/** @fn object xpost_object_cvx(object obj)
 *  @brief Return object, with executable attribute set to executable.
 */
object xpost_object_cvx (object obj);

/** @fn object xpost_object_cvlit(object obj)
 *  @brief Return object, with executable attribute set to literal.
 */
object xpost_object_cvlit (object obj);


/*
   Debugging dump
 */

/** @fn void xpost_object_dump(object obj)
 *  @brief print a dump of the object contents to stdout
 */
void xpost_object_dump (object obj);

/**
 * @}
 */

#endif
