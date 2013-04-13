typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
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
	_(string)  \
/* #def TYPES */

#define AS_TYPE(_) \
	_ ## type ,
enum {
	TYPES(AS_TYPE)
	NTYPES
};

#define AS_STR(_) #_ ,
extern
char *types[] /*= { TYPES(AS_STR) "invalid"}*/ ;

typedef struct {
	word tag;
	word pad0;
	word pad1;
	word pad2;
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

typedef union {
	word tag;

	mark_ null_;
	mark_ mark_;
	int_ int_;
	real_ real_;
	comp_ comp_;
} object;

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

object consint(integer i);
object consreal(real r);
void dumpobject(object o);

