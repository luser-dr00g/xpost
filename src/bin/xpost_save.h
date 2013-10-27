#ifndef XPOST_V_H
#define XPOST_V_H

/* save/restore
   Each mfile has a special entity (VS) which holds the address
   of the "save stack". This stack holds save objects.

   The save object contains an address of a(nother) stack,
   this one containing saverec_ structures.
   A saverec_ object contains 2 entity numbers, one the source,
   the other the copy, of the "saved" array or dictionary.

   stashed and stash are the interfaces used by composite objects
   to check-if-copying-is-necessary
   and copy-the-value-and-add-saverec-to-current-savelevel-stack

   mem[mtab[VS].adr] = Master Save stack
   -- save object = { lev=0, stk=... }
   -- save object = { lev=1, stk=... }
      -- saverec
      -- saverec
   -- save object = { lev=2, stk=... }  <-- top of VS, current savelevel stack
      mem[save.save_.stk] = Save Object's stack
      -- saverec
      -- saverec
      -- saverec = { src=foo_ent, cpy=bar_ent }

   */

void initsave(mfile *mem);
Xpost_Object save(mfile *mem);
unsigned stashed(mfile *mem, unsigned ent);
unsigned copy(mfile *mem, unsigned ent);
void stash(mfile *mem, unsigned tag, unsigned pad, unsigned ent);
void restore(mfile *mem);

#endif
