#ifndef XPOST_OB_H
#define XPOST_OB_H

/* objects
   define the basic 8-byte object structure
   */

#ifdef LARGEOBJECT
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
enum {
    TYPES(AS_TYPE)
    NTYPES
};

#define AS_STR(_) \
    #_ ,
#define AS_TYPE_STR(_) \
    #_ "type" ,
extern
char *types[] /*= { TYPES(AS_TYPE_STR) "invalid"}*/ ;

enum tagdata {
    TYPEMASK = 0x000F,
    FVALID =   0x0010, /* for 'anytype' operator pattern (7/9/2013: TODO wtf is this??) */
    FACCESS =  0x0060,
    FACCESSO = 5,  /* bitwise offset of the ACCESS field */
    FLIT =     0x0080,
    FBANK =    0x0100, /* 0=local, 1=global */
 EXTENDEDINT = 0x0200,
EXTENDEDREAL = 0x0400,
};

enum faccess {
    noaccess    = 0,
    executeonly = 1,
    readonly    = 2,
    unlimited   = 3,
};

/* To avoid too many structures, many types use .mark_.padw
   to hold an unsigned value (eg. operatortype, nametype, filetype).
   Of, course if a type needs to use pad0, that's a sign that it needs
   its own struct.
   */
typedef struct {
    word tag;
    word pad0;
    dword padw;
} mark_;

typedef struct {
    word tag;
    word pad;
    integer val;
} int_;

typedef struct {
    word tag;
    word pad;
    real val;
} real_;

typedef struct {
    word tag;
    word sign_exp;
    dword fraction;
} extended_;

typedef struct {
    word tag;
    word sz;
    word ent;
    word off;
} comp_;

typedef struct {
    word tag;
    word lev;
    dword stk;
} save_;

typedef struct {
    dword src;
    dword cpy;
} saverec_; /* overlays an object so it can be stacked */
/* but only on the "save" stack, not user visible. */

typedef struct {
    word tag;
    word off;
    void *ptr;
} glob_;

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

int type(object o);
int isx(object o);
int islit(object o);
int faccess(object o);
object setfaccess(object o, int access);
int isreadable(object o);
int iswriteable(object o);

#define DECLARE_SINGLETON(_) extern object _;
#define DEFINE_SINGLETON(_) object _ = { AS_TYPE(_) };
#define SINGLETONS(_) \
    _(invalid) \
    _(null) \
    _(mark) \
/* #def SINGLETONS */

SINGLETONS(DECLARE_SINGLETON)

object consbool(bool b);
object consint(integer i);
object consreal(real r);

object cvx(object o);
object cvlit(object o);

void dumpobject(object o);

#endif
