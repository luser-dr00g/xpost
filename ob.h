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
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;
typedef int integer;
typedef float real;
typedef dword addr;
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
extern
char *types[] /*= { TYPES(AS_STR) "invalid"}*/ ;

enum tagdata {
    TYPEMASK = 0x000F,
    FVALID =   0x0010, /* for 'anytype' operator pattern (7/9/2013: TODO wtf is this??) */
    FACCESS =  0x0060,
    FACCESSO = 5,  /* bitwise offset of the ACCESS field */
    FLIT =     0x0080,
    FBANK =    0x0100, /* 0=local, 1=global */
};

enum faccess {
    noaccess    = 0,
    executeonly = 1,
    readonly    = 2,
    unlimited   = 3,
};

/* To avoid too many structures, many types use .mark_.padw 
   to hold an unsigned value (eg. operatortype, nametype).
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

typedef union {
    word tag;

    //mark_ null_;
    mark_ mark_;
    int_ int_;
    real_ real_;
    comp_ comp_;
    save_ save_;
    saverec_ saverec_;
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

