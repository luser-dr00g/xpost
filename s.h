
/* must include ob.h */ 
/*typedef long long object;*/

#define STACKSEGSZ 1

typedef struct {
	unsigned nextseg;
	unsigned top;
	object data[STACKSEGSZ];
} stack;

unsigned initstack(mfile *mem);

void dumpstack(mfile *mem, unsigned stackadr);

void sfree(mfile *mem, unsigned stackadr);

unsigned count(mfile *mem, unsigned stackadr);

void push(mfile *mem, unsigned stackadr, object o);

object top(mfile *mem, unsigned stackadr, integer i);
void pot(mfile *mem, unsigned stackadr, integer i, object o);

object bot(mfile *mem, unsigned stackadr, integer i);
void tob(mfile *mem, unsigned stacadr, integer i, object o);

object pop(mfile *mem, unsigned stackadr);
/*int pop(mfile *mem, unsigned stackadr, object *po);*/ 


