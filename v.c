//#define TESTMODULE

/* define MMAP and MREMAP for Linux */
/* define MMAP without MREMAP should work for Irix (using AUTOGROW) */
/* no MREMAP under cygwin (we'll see how AUTOGROW handles...)*/
/* define neither to use malloc/realloc/free */
#define MMAP
//#define MREMAP
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef MMAP
#include<sys/mman.h>
#endif

/* placeholder error function */
/* ultimately, this will do a longjmp back to the central loop */
void error(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

unsigned pgsz /*= getpagesize()*/; /*=4096 (usually on 32bit)*/

typedef struct {
	unsigned char *base;
	unsigned used;
	unsigned max;
} mfile;

void dumpmfile(mfile *mem){
	printf("{mfile: base = %p, "
			"used = 0x%x (%u), "
			"max = 0x%x (%u)}\n",
			mem->base,
			mem->used, mem->used,
			mem->max, mem->max);
}

/* initialize the memory file */
void initmem(mfile *mem){
#ifdef MMAP
	mem->base = mmap(NULL, pgsz,
			PROT_READ|PROT_WRITE,
			MAP_PRIVATE
# ifndef MREMAP
			|MAP_AUTOGROW
# endif
			|MAP_ANONYMOUS, -1, 0);
	if (mem->base == MAP_FAILED)
#else
	mem->base = malloc(pgsz);
	if (mem->base == NULL)
#endif
		error("unable to initialize memory file");
	mem->used = 0;
	mem->max = pgsz;
}

/* destroy the memory file */
void exitmem(mfile *mem){
#ifdef MMAP
	munmap(mem->base, mem->max);
#else
	free(mem->base);
#endif
	mem->base = NULL;
	mem->used = 0;
	mem->max = 0;
}

void growmem(mfile *mem, unsigned sz){
	void *tmp;
	if (sz < pgsz) sz = pgsz;
	else sz = (sz/pgsz + 1) * pgsz;
	sz += mem->max;
#ifdef MMAP
# ifdef MREMAP
	tmp = mremap(mem->base, mem->max, sz, MREMAP_MAYMOVE);
# else
	tmp = mem->base; /* without mremap, rely on MAP_AUTOGROW */
# endif
	if (tmp == MAP_FAILED)
#else
	tmp = realloc(mem->base, sz);
	if (tmp == NULL)
#endif
		error("unable to grow memory");
	mem->base = tmp;
	mem->max = sz;
}

/* allocate memory, returns offset in memory file */
unsigned mfalloc(mfile *mem, unsigned sz){
	unsigned adr = mem->used;
	if (sz + mem->used > //=
			mem->max) growmem(mem,sz);
	mem->used += sz;
	bzero(mem->base+adr, sz);
	/* bus error with mremap(SHARED,ANON)! */
	return adr;
}


#define TABSZ 1000
typedef struct {
	unsigned nexttab; /* next table in chain */
	unsigned nextent; /* next slot in table, or TABSZ if none */
	struct {
		unsigned adr;
		unsigned sz;
		/* add fields here for ref counts or marks */
	} tab[TABSZ];
} mtab;

/* allocate and initialize a new table */
unsigned initmtab(mfile *mem){
	unsigned adr;
	adr = mfalloc(mem, sizeof(mtab));
	mtab *tab = (void *)(mem->base + adr);
	tab->nexttab = 0;
	tab->nextent = 0;
	return adr;
}

/* allocate memory, returns table index */
unsigned mtalloc(mfile *mem, unsigned mtabadr, unsigned sz){
	mtab *tab = (void *)(mem->base + mtabadr);
	if (tab->nextent >= TABSZ)
		return mtalloc(mem, tab->nexttab, sz);
	else {
		unsigned ent = tab->nextent;
		++tab->nextent;

		tab->tab[ent].adr = mfalloc(mem, sz);
		tab->tab[ent].sz = sz;

		if (tab->nextent == TABSZ){
			tab->nexttab = initmtab(mem);
		}
		return ent;
	}
}

/* fetch a value from a composite object */
void get(mfile *mem,
		unsigned ent, unsigned offset, unsigned sz,
		void *dest){
	mtab *tab;
	unsigned mtabadr = 0;
	tab = (void *)(mem->base + mtabadr);
	while (ent >= TABSZ) {
		mtabadr = tab->nexttab;
		tab = (void *)(mem->base + mtabadr);
		ent -= TABSZ;
	}

	if (offset*sz + sz > tab->tab[ent].sz)
		error("get: out of bounds");

	memcpy(dest, mem->base + tab->tab[ent].adr + offset*sz, sz);
}

/* put a value into a composite object */
void put(mfile *mem,
		unsigned ent, unsigned offset, unsigned sz,
		void *src){
	mtab *tab;
	unsigned mtabadr = 0;
	tab = (void *)(mem->base + mtabadr);
	while (ent >= TABSZ){
		mtabadr = tab->nexttab;
		tab = (void *)(mem->base + mtabadr);
		ent -= TABSZ;
	}

	if (offset*sz + sz > tab->tab[ent].sz)
		error("put: out of bounds");

	memcpy(mem->base + tab->tab[ent].adr + offset*sz, src, sz);
}

#ifdef TESTMODULE

mfile mem;

/* initialize everything */
void init(void){
	pgsz = getpagesize();
	initmem(&mem);
	(void)initmtab(&mem); /* create mtab at address zero */
}

void xit(void){
	exitmem(&mem);
}

int main(){
	init();
	unsigned ent;
	int seven = 7;
	int ret;
	ent = mtalloc(&mem, 0, sizeof seven);
	put(&mem, ent, 0, sizeof seven, &seven);
	get(&mem, ent, 0, sizeof seven, &ret);
	printf("put %d, got %d\n", seven, ret);

	unsigned ent2;
	ent2 = mtalloc(&mem, 0, 8*sizeof seven);
	put(&mem, ent2, 6, sizeof seven, &seven);
	get(&mem, ent2, 6, sizeof seven, &ret);
	printf("put %d in slot 7, got %d\n", seven, ret);
	//get(&mem, ent2, 9, sizeof seven, &ret);
	//printf("attempted to retrieve element 10 from an 8-element array, got %d\n", ret);

	unsigned ent3;
	char str[] = "beads in buddha's necklace";
	char sret[sizeof str];
	ent3 = mtalloc(&mem, 0, strlen(str)+1);
	put(&mem, ent3, 0, sizeof str, str);
	get(&mem, ent3, 0, sizeof str, sret);
	printf("stored and retrieved %s\n", sret);

	xit();
	return 0;
}
#endif
