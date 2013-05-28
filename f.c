#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "m.h"
#include "ob.h"

/* construct a file object.
   set the tag,
   use the "doubleword" field as a "pointer" (ent),
   allocate a FILE *,
   install the FILE *,
   return object.  */
object consfile(mfile *mem, FILE *fp) {
	object f;
	f.tag = filetype;
	//f.mark_.padw = mtalloc(mem, 0, sizeof(FILE *));
	f.mark_.padw = gballoc(mem, sizeof(FILE *));
	put(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
	return f;
}

/* pinch-off a tmpfile containing one line from stdin. */
FILE *lineedit() {
	FILE *fp;
	int c;

	c = fgetc(stdin);
	if (c == EOF) error("undefinedfilename");
	fp = tmpfile();
	while (c != EOF && c != '\n') {
		fputc(c, fp);
		c = fgetc(stdin);
	}
	fseek(fp, 0, SEEK_SET);
	return fp;
}

enum { MAXNEST = 20 };

/* pinch-off a tmpfile containing one "statement" from stdin. */
FILE *statementedit() {
	FILE *fp;
	int c;
	char nest[MAXNEST]; /* any of {(< waiting for matching >)} */
	int defer = -1; /* defer is a flag (-1 == false)
					   and an index into nest[] */
	c = fgetc(stdin);
	if (c == EOF) error("undefinedfilename");
	fp = tmpfile();
	do {
		if (defer > -1) {
			if (defer > MAXNEST) error("syntaxerror");
			switch(nest[defer]) { /* what's the innermost nest? */
			case '{': /* within a proc, can end proc or begin proc, string, hex */
				switch (c) {
				case '}': --defer; break;
				case '{':
				case '(':
				case '<': nest[++defer] = c; break;
				} break;
			case '(': /* within a string, can begin or end nested paren */
				switch (c) {
				case ')': --defer; break;
				case '(': nest[++defer] = c; break;
				case '\\': fputc(c, fp);
						   c = fgetc(stdin);
						   if (c == EOF) goto done;
						   goto next;
				} break;
			case '<': /* hexstrings don't nest, can only end it */
				if (c == '>') --defer; break;
			}
		} else switch (c) { /* undefined, can begin any structure */
		case '{':
		case '(':
		case '<': nest[++defer] = c; break;
		case '\\': fputc(c, fp);
				   c = fgetc(stdin); break;
		}
		if (c == '\n') {
			if (defer == -1) goto done;
			{ /* sub-prompt */
				int i;
				for (i=0; i <= defer; i++)
					putchar(nest[i]);
				fputs(".:", stdout);
				fflush(NULL);
			}
		}
next:
		fputc(c, fp);
		c = fgetc(stdin);
	} while(c != EOF);
done:
	fseek(fp, 0, SEEK_SET);
	return fp;
}

/* check for "special" filenames,
   fallback to fopen. */
object fileopen(mfile *mem, char *fn, char *mode) {
	object f;
	f.tag = filetype;

	if (strcmp(fn, "%stdin")==0) {
		if (strcmp(mode, "r")!=0) error("invalidfileaccess");
		f = consfile(mem, stdin);
	} else if (strcmp(fn, "%stdout")==0) {
		if (strcmp(mode, "w")!=0) error("invalidfileaccess");
		f = consfile(mem, stdout);
	} else if (strcmp(fn, "%stderr")==0) {
		if (strcmp(mode, "w")!=0) error("invalidfileaccess");
		f = consfile(mem, stderr);
	} else if (strcmp(fn, "%lineedit")==0) {
		f = consfile(mem, lineedit());
	} else if (strcmp(fn, "%statementedit")==0) {
		f = consfile(mem, statementedit());
	} else {
		FILE *fp;
		fp = fopen(fn, mode);
		if (fp == NULL) {
			switch (errno) {
			case EACCES: error("invalidfilename"), exit(EXIT_FAILURE);
			case ENOENT: error("undefinedfilename"), exit(EXIT_FAILURE);
			default: error("unregistered"), exit(EXIT_FAILURE);
			}
		}
		f = consfile(mem, fp);
	}

	f.tag |= FLIT;
	return f;
}

/* yield the FILE* from a filetype object */
FILE *filefile(mfile *mem, object f) {
	FILE *fp;
	get(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
	return fp;
}

/* make sure the FILE* is not null */
bool filestatus(mfile *mem, object f) {
	return filefile(mem, f) != NULL;
}

/* call fstat. */
long filebytesavailable(mfile *mem, object f) {
	FILE *fp;
	struct stat sb;
	fp = filefile(mem, f);
	if (!fp) return -1;
	fstat(fileno(fp), &sb);
	if (sb.st_size > LONG_MAX)
		return LONG_MAX;
	return (long)sb.st_size;
}

/* close the file,
   NULL the FILE*. */
void fileclose(mfile *mem, object f) {
	FILE *fp;
	fp = filefile(mem, f);
	if (fp) {
		fclose(fp);
		fp = NULL;
		put(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
	}
}

/* if the file is valid,
   read a byte. */
object fileread(mfile *mem, object f) {
	if (!filestatus(mem, f)) error("ioerror");
	return consint(fgetc(filefile(mem, f)));
}

/* if the file is valid,
   write a byte. */
void filewrite(mfile *mem, object f, object b) {
	if (!filestatus(mem, f)) error("ioerror");
	if (fputc(b.int_.val, filefile(mem, f)) == EOF)
		error("ioerror");
}

