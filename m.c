#include "m.h"


#include <ctype.h> /* isprint */
#include <stdlib.h> /* exit free malloc realloc */
#include <stdio.h> /* fprintf printf perror */
#include <string.h> /* memset */
#include <unistd.h> /* getpagesize */

#include <sys/stat.h> /* open */
#include <fcntl.h> /* open */

/* placeholder error function */
/* ultimately, this will do a longjmp back to the central loop */
void error(char *msg) {
	fprintf(stderr, "%s\n", msg);
	perror("last system error:");
	exit(EXIT_FAILURE);
}

unsigned pgsz /*= getpagesize()*/ = 4096;

/*
typedef struct {
	unsigned char *base;
	unsigned used;
	unsigned max;
} mfile;
*/

/* dump mfile details to stdout */
void dumpmfile(mfile *mem){
	printf("{mfile: base = %p, "
			"used = 0x%x (%u), "
			"max = 0x%x (%u)}\n",
			mem->base,
			mem->used, mem->used,
			mem->max, mem->max);
	unsigned u;
	for (u=0; u < mem->used; u++) {
		if (u%16 == 0) {
			if (u != 0) {
				unsigned v;
				for (v= u-16; v < u; v++) {
					putchar( isprint(mem->base[v])?
							mem->base[v] : '.');
				}
			}
			printf("\n%06u %04x: ", u, u);
		}
		printf("%02x ", (unsigned) mem->base[u]);
	}
	puts("");
}

/* memfile exists in path */
int getmemfile(char *fname){
	int fd;
	fd = open(
			fname, //"x.mem",
			O_RDWR | O_CREAT );
	if (fd == -1)
		perror(fname);
	return fd;
}

/* initialize the memory file */
void initmem(mfile *mem, char *fname){
	int fd = -1;
	struct stat buf;
	size_t sz = pgsz;

	if (fname) {
		fd = getmemfile(fname);
	}
	mem->fd = fd;
	if (fd != -1){
		fstat(fd, &buf);
		sz = buf.st_size;
		if (sz < pgsz) sz = pgsz;
	}

#ifdef MMAP
	mem->base = mmap(NULL,
			sz,
			PROT_READ|PROT_WRITE,
			MAP_SHARED //MAP_PRIVATE
# ifndef MREMAP
			|MAP_AUTOGROW
# endif
			| (fd == -1? MAP_ANONYMOUS : 0) , fd, 0);
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
	msync(mem->base, mem->used, MS_SYNC);
#ifdef MMAP
	munmap(mem->base, mem->max);
#else
	free(mem->base);
#endif
	mem->base = NULL;
	mem->used = 0;
	mem->max = 0;

	if (mem->fd != -1) {
		/* int ftruncate(int fd, off_t length); */
		/* The truncate() and ftruncate() functions cause the
		   regular file named by path or referenced by fd to
		   be truncated to a size of precisely length bytes. */
		close(mem->fd);
	}
}

void growmem(mfile *mem, unsigned sz){
	void *tmp;
	printf("growmem: %p %u\n", mem, sz);
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
	if (sz) {
		if (sz + mem->used >=
				mem->max) growmem(mem,sz);
		mem->used += sz;
		memset(mem->base+adr, 0, sz);  //bzero(mem->base+adr, sz);
		/* bus error with mremap(SHARED,ANON)! */
	}
	return adr;
}


#if 0
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
#endif

/* dump mtab details to stdout */
void dumpmtab(mfile *mem, unsigned mtabadr){
	mtab *tab = (void *)(mem->base + mtabadr);
	printf("nexttab: 0x%04x\n", tab->nexttab);
	printf("nextent: %u\n", tab->nextent);
	unsigned i;
	for (i=0; i<tab->nextent; i++) {
		printf("%d: %u %04x [%u] %s %d %d %d\n",
				i,
				tab->tab[i].adr, tab->tab[i].adr,
				tab->tab[i].sz,
				tab->tab[i].mark & MARKM ?"#":"_",
				(tab->tab[i].mark & RFCTM) >> RFCTO,
				(tab->tab[i].mark & LLEVM) >> LLEVO,
				(tab->tab[i].mark & TLEVM) >> TLEVO );
		unsigned u;
		for (u=0; u < tab->tab[i].sz; u++) {
			printf(" %02x", (unsigned)mem->base[
					tab->tab[i].adr + u ] );
		}
		puts("");
	}
	if (tab->nextent == TABSZ) dumpmtab(mem, tab->nexttab);
}


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
	int ntab = 0;
	while (tab->nextent >= TABSZ) {
		tab = (void *)(mem->base + tab->nexttab);
		++ntab;
	}

	unsigned ent = tab->nextent;
	++tab->nextent;

	tab->tab[ent].adr = mfalloc(mem, sz);
	ent += ntab*TABSZ; //recalc
	findtabent(mem, &tab, &ent); //recalc
	tab->tab[ent].sz = sz;

	if (tab->nextent == TABSZ){
		tab->nexttab = initmtab(mem);
	}
	return ent + ntab*TABSZ;
}

/* find the table and relative entity index for an absolute entity index */
void findtabent(mfile *mem, mtab **atab, unsigned *aent) {
	*atab = (void *)(mem->base);
	while (*aent >= TABSZ) {
		*atab = (void *)(mem->base + (*atab)->nexttab);
		*aent -= TABSZ;
	}
}

/* get the address from an entity */
unsigned adrent(mfile *mem, unsigned ent) {
	mtab *tab = (void *)(mem->base);
	findtabent(mem,&tab,&ent);
	return tab->tab[ent].adr;
}

/* get the size of an entity */
unsigned szent(mfile *mem, unsigned ent) {
	mtab *tab = (void *)(mem->base);
	findtabent(mem,&tab,&ent);
	return tab->tab[ent].sz;
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

	if (offset*sz /*+ sz*/ > tab->tab[ent].sz)
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

	if (offset*sz /*+ sz*/ > tab->tab[ent].sz)
		error("put: out of bounds");

	memcpy(mem->base + tab->tab[ent].adr + offset*sz, src, sz);
}

#ifdef TESTMODULE

mfile mem;

/* initialize everything */
void init(void){
	pgsz = getpagesize();
	initmem(&mem, "x.mem");
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

	printf("\n^test m.c\n");
	//printf("getmemfile: %d\n", getmemfile());

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

	dumpmtab(&mem, 0);
	xit();
	return 0;
}
#endif
