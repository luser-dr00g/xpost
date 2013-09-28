#ifndef XPOST_OB_H
#define XPOST_OB_H

/**
 * @file ob.h
 * @brief The file defines the basic 8-byte object structure.
 * @defgroup xpost_object Object structure
 *
 * @{
*/

#ifdef WANT_LARGE_OBJECT
typedef unsigned char byte;
typedef uint32_t word;
typedef uint64_t dword;
typedef uint128_t qword;
typedef int64_t integer;
typedef double real;
typedef dword addr;
#else
typedef unsigned char byte;   //assumes ==8bit
typedef unsigned short word;  //assumes ==16bit
typedef unsigned long dword;  //assumes >=32bit
typedef unsigned long long qword; //assumes >=64bit
typedef int integer;
typedef float real;  //assumes 32bit
typedef dword addr;  //hmm... should probably use this more. :)
#endif

/*! \def TYPES
    \brief x-macro for defining enum of typenames and string-table
           char *types[];
*/

#define TYPES(_) \
    _(invalid) \
    _(null)    \
    _(mark)    \
    _(integer) \
    _(real)    \
    _(array)   \
    _(dict)    \
    _(file)    \
    _(operator) \
    _(save)    \
    _(name)    \
    _(boolean) \
    _(context) \
    _(extended) \
    _(glob) \
    _(string)  \
/* #def TYPES */

#define AS_TYPE(_) \
    _ ## type ,
enum types {
    TYPES(AS_TYPE)
    NTYPES
};

/*! \var char *types[]
    \brief a table of strings keyed to the type enum
*/

#define AS_STR(_) \
    #_ ,
#define AS_TYPE_STR(_) \
    #_ "type" ,
extern
char *types[] /*= { TYPES(AS_TYPE_STR) "invalid"}*/ ;

/**
 * @enum tagdata
 * @brief Bitmasks and bitshift-positions for the flags in the tag.
 */
enum tagdata {
    TYPEMASK = 0x000F,
    FVALID =   0x0010, /**< for 'anytype' operator pattern (7/9/2013: TODO wtf is this??) */
    FACCESS =  0x0060,
    FACCESSO = 5,  /**< bitwise offset of the ACCESS field */
    FLIT =     0x0080,
    FBANK =    0x0100, /* 0=local, 1=global */
    EXTENDEDINT = 0x0200,
    EXTENDEDREAL = 0x0400,
};

/*! \def enum faccess
    \brief valid values for the ACCESS bitfield
*/

enum faccess {
    noaccess    = 0,
    executeonly = 1,
    readonly    = 2,
    unlimited   = 3,
};

/*! \def typedef struct { } mark_
    \brief a generic object: 2 unsigned shorts and an unsigned long

   To avoid too many structures, many types use .mark_.padw
   to hold an unsigned value (eg. operatortype, nametype, filetype).
   Of, course if a type needs to use pad0, that's a sign that it needs
   its own struct.
*/

typedef struct {
    word tag;
    word pad0;
    dword padw;
} mark_;

/*! \def typedef struct { } int_
    \brief the integertype object
*/

typedef struct {
    word tag;
    word pad;
    integer val;
} int_;

/*! \def typedef struct { } real_
    \brief the realtype object
*/

typedef struct {
    word tag;
    word pad;
    real val;
} real_;

/*! \def typedef struct { } extended_
    \brief a combined-integer-real for use in dictionaries as number keys
*/

typedef struct {
    word tag;
    word sign_exp;
    dword fraction;
} extended_;

/*! \def typedef struct { } comp_
    \brief the composite object structure, used for strings, arrays, dicts
*/

typedef struct {
    word tag;
    word sz;
    word ent;
    word off;
} comp_;

/*! \def typedef struct { } save_
    \brief the savetype object, for both user and on the save stack
*/

typedef struct {
    word tag;
    word lev;
    dword stk;
} save_;

/*! \def typedef struct { } saverec_
    \brief saverecs occupy the "current save stack"
        refered to by the stk field of a save object
*/

typedef struct {
    dword src;
    dword cpy;
} saverec_; /* overlays an object so it can be stacked */
/* but only on the "save" stack, not user visible. */

/*! \def typedef struct { } glob_
    \brief globtype object exists only for passing between
        iterations of filenameforall

    There are no constructors for this type. It has no use 
    outside the filenameforall looping construct.
*/

typedef struct {
    word tag;
    word off;
    void *ptr;
} glob_;

/*! \def typedef union { } object
    \brief the top-level object union

    The tag word overlays the tag words in each subtype, so it can
    be used to determine an object's type (using the type() function
    which masks-off any flags in the tag).
*/

typedef union {
    word tag;

    //mark_ null_;
    mark_ mark_;
    int_ int_;
    real_ real_;
    extended_ extended_;
    comp_ comp_;
    save_ save_;
    saverec_ saverec_;
    glob_ glob_;
} object;

/**
 * @brief Return the object's type, ie. the tag with flags masked-off.
 *
 * @param o The object.
 * @return The type of the object.
 *
 * This function returns the type of the object @p o, that is the tag
 * with flags masked-off.
*/
int type(object o);

/**
 * @brief Check whether the object is executable or not.
 *
 * @param o The object.
 * @return 1 if the object is executabe, 0 otherwise.
 *
 * This function returns 1 if the object @p o is executable, 0
 * otherwise.
 */
int isx(object o);

/**
 * @brief Check whether the object is literal or not.
 *
 * @param o The object.
 * @return 1 if the object is literal, 0 otherwise.
 *
 * This function returns 1 if the object @p o is literal, 0
 * otherwise.
 */
int islit(object o);

/*! \fn int faccess(object o)
    \brief return the access-field from the object's tag
*/

int faccess(object o);

/*! \fn object setfaccess(object o, int access)
    \brief return object with access-field set to access
*/

object setfaccess(object o, int access);

/*! \fn int isreadable(object o)
    \brief return non-zero if access-field indicates read-access
*/

int isreadable(object o);

/*! \fn int iswriteable(object o)
    \brief return non-zero if access-field indicates write-access
*/

int iswriteable(object o);

/*! \def SINGLETONS
    \brief certain simple objects exist as global template variables
        rather than do-nothing constructors
*/

#define DECLARE_SINGLETON(_) extern object _;
#define DEFINE_SINGLETON(_) object _ = { AS_TYPE(_) };
#define SINGLETONS(_) \
    _(invalid) \
    _(null) \
    _(mark) \
/* #def SINGLETONS */

SINGLETONS(DECLARE_SINGLETON)

/*! \fn object consbool(bool b)
    \brief construct a boolean object with value b
*/

object consbool(bool b);

/*! \fn object consint(integer i)
    \brief construct an integer object with value i
*/

object consint(integer i);

/*! \fn object consreal(real r)
    \brief construct a real object with value r
*/

object consreal(real r);

/*! \fn object cvx(object o)
    \brief return object, made executable
*/

object cvx(object o);

/*! \fn object cvlit(object o)
    \brief return object, made literal
*/

object cvlit(object o);

/*! \fn void dumpobject(object o)
    \brief print a dump of the object contents to stdout
*/

void dumpobject(object o);

/**
 * @}
 */

#endif
