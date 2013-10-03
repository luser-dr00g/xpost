#ifndef XPOST_S_H
#define XPOST_S_H

/* stacks
   Stacks consist of a chain of tables, much like mtab chains.
   */

/* must include ob.h */
/*typedef long long object;*/

#define STACKSEGSZ 20

typedef struct {
    unsigned nextseg;
    unsigned top;
    object data[STACKSEGSZ];
} stack;

/* create a stack data structure. returns vm address */
unsigned initstack(mfile *mem);

/* dump the contents of a stack to stdout using dumpobject */
void dumpstack(mfile *mem, unsigned stackadr);

/* free a stack, and all succeeding segments */
void sfree(mfile *mem, unsigned stackadr);

/* count elements on stack */
unsigned count(mfile *mem, unsigned stackadr);

/* push an object on top of the stack */
void push(mfile *mem, unsigned stackadr, object o);

/* index the stack from the top down, fetching object */
object top(mfile *mem, unsigned stackadr, integer i);
/* index the stack from the top down, replacing object */
void pot(mfile *mem, unsigned stackadr, integer i, object o);

/* index the stack from the bottom up, fetching object */
object bot(mfile *mem, unsigned stackadr, integer i);
/* index the stack from the bottom up, replacing object */
void tob(mfile *mem, unsigned stacadr, integer i, object o);

/* pop the stack. remove and return top object */
object pop(mfile *mem, unsigned stackadr);
/*int pop(mfile *mem, unsigned stackadr, object *po);*/

#endif
