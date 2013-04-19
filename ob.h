typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned long long qword;
typedef int integer;
typedef float real;
typedef dword addr;

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
	_(save)	   \
	_(name)	   \
	_(boolean) \
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
	FVALID =   0x0010, /* for 'anytype' operator pattern */
	FACCESS =  0x0060,
	FACCESSO = 5,  /* bitwise offset of the ACCESS field */
	FLIT =     0x0080,
	FBANK =    0x0100, /* 0=local, 1=global */
};

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
	unsigned stk;
} save_;

typedef struct {
	unsigned src;
	unsigned cpy;
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

integer type(object o);
integer isx(object o);
integer islit(object o);

#define DECLARE_SINGLETON(_) extern object _;
#define DEFINE_SINGLETON(_) object _ = { AS_TYPE(_) };
#define SINGLETONS(_) \
	_(invalid) \
	_(null) \
	_(mark) \
/* #def SINGLETONS */

SINGLETONS(DECLARE_SINGLETON)

//extern object invalid /*= { invalidtype }*/;
//extern object mark /*= { marktype }*/;
//extern object null /*= { nulltype }*/;

object consbool(bool b);
object consint(integer i);
object consreal(real r);
void dumpobject(object o);

