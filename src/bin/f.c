#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "m.h"  // files store FILE*s in (local) mfile
#include "ob.h"  // files are objects
#include "gc.h"  // files data allocated with gballoc
#include "itp.h"  // interpreter
#include "err.h"  // file functions may throw errors
#include "f.h"  // double-check prototypes

#ifdef __MINGW32__
static FILE *
f_tmpfile()
{
  char *buf;
  const char *name;
  const char *tmpdir;
  size_t l1;
  size_t l2;

  tmpdir = getenv("TEMP");
  if (!tmpdir)
    tmpdir = getenv("TMP");
  if (!tmpdir)
    return NULL;

  name = tmpnam(NULL);
  /* name points to a static buffer, so no need to check it */

  l1 = strlen(tmpdir);
  l2 = strlen(name);
  buf = alloca(l1 + l2 + 1);
  memcpy(buf, tmpdir, l1);
  memcpy(buf + l1, name, l2);
  buf[l1 + l2] = '\0';

  return fopen(buf, "wbD");
}
#else
# define f_tmpfile tmpfile
#endif


/* filetype objects use a slightly different interpretation
   of the access flags.
   'unlimited' designates a writable file
   'readonly' designates a readable file
   The only oddity here is that 'unlimited' means "not readable".
   */

/* construct a file object.
   set the tag,
   use the "doubleword" field as a "pointer" (ent),
   allocate a FILE *,
   install the FILE *,
   return object.  */
object consfile(mfile *mem,
        /*@NULL@*/ FILE *fp)
{
    object f;

    f.tag = filetype | (unlimited << FACCESSO);
    //f.mark_.padw = mtalloc(mem, 0, sizeof(FILE *));
    f.mark_.padw = gballoc(mem, sizeof(FILE *));
    put(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
    return f;
}

/* pinch-off a tmpfile containing one line from file. */
/*@null@*/
FILE *lineedit(FILE *in)
{
    FILE *fp;
    int c;

    c = fgetc(in);
    if (c == EOF) error(undefinedfilename, "%lineedit");
    fp = f_tmpfile();
    if (fp == NULL) { error(ioerror, "tmpfile() returned NULL"); return NULL; }
    while (c != EOF && c != '\n') {
        (void)fputc(c, fp);
        c = fgetc(in);
    }
    fseek(fp, 0, SEEK_SET);
    return fp;
}

enum { MAXNEST = 20 };

/* pinch-off a tmpfile containing one "statement" from file. */
/*@null@*/
FILE *statementedit(FILE *in)
{
    FILE *fp;
    int c;
    char nest[MAXNEST] = {0}; /* any of {(< waiting for matching >)} */
    int defer = -1; /* defer is a flag (-1 == false)
                       and an index into nest[] */

    c = fgetc(in);
    if (c == EOF) error(undefinedfilename, "%statementedit");
    fp = f_tmpfile();
    if (fp == NULL) { error(ioerror, "tmpfile() returned NULL"); return NULL; }
    do {
        if (defer > -1) {
            if (defer > MAXNEST) error(syntaxerror, "syntaxerror");
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
                           c = fgetc(in);
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
                   c = fgetc(in); break;
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
        c = fgetc(in);
    } while(c != EOF);
done:
    fseek(fp, 0, SEEK_SET);
    return fp;
}

/* check for "special" filenames,
   fallback to fopen. */
object fileopen(mfile *mem,
        char *fn,
        char *mode)
{
    object f;
    f.tag = filetype;

    if (strcmp(fn, "%stdin")==0) {
        if (strcmp(mode, "r")!=0) error(invalidfileaccess, "fileopen");
        f = consfile(mem, stdin);
        f.tag &= ~FACCESS;
        f.tag |= (readonly << FACCESSO);
    } else if (strcmp(fn, "%stdout")==0) {
        if (strcmp(mode, "w")!=0) error(invalidfileaccess, "fileopen");
        f = consfile(mem, stdout);
    } else if (strcmp(fn, "%stderr")==0) {
        if (strcmp(mode, "w")!=0) error(invalidfileaccess, "fileopen");
        f = consfile(mem, stderr);
    } else if (strcmp(fn, "%lineedit")==0) {
        f = consfile(mem, lineedit(stdin));
        f.tag &= ~FACCESS;
        f.tag |= (readonly << FACCESSO);
    } else if (strcmp(fn, "%statementedit")==0) {
        f = consfile(mem, statementedit(stdin));
        f.tag &= ~FACCESS;
        f.tag |= (readonly << FACCESSO);
    } else {
        FILE *fp;
        fp = fopen(fn, mode);
        if (fp == NULL) {
            switch (errno) {
            case EACCES: error(invalidfileaccess, "fileopen"); break;
            case ENOENT: error(undefinedfilename, "fileopen"); break;
            default: error(unregistered, "fileopen"); break;
            }
        }
        f = consfile(mem, fp);
        if (strcmp(mode, "r")==0){
            f.tag &= ~FACCESS;
            f.tag |= (readonly << FACCESSO);
        }
    }

    f.tag |= FLIT;
    return f;
}

/* adapter:
           FILE* <- filetype object
   yield the FILE* from a filetype object */
FILE *filefile(mfile *mem,
               object f)
{
    FILE *fp;
    get(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
    return fp;
}

/* make sure the FILE* is not null */
bool filestatus(mfile *mem,
                object f)
{
    return filefile(mem, f) != NULL;
}

/* call fstat. */
long filebytesavailable(mfile *mem,
                        object f)
{
    int ret;
    FILE *fp;
    struct stat sb;
    long sz, pos;

    fp = filefile(mem, f);
    if (!fp) return -1;
    ret = fstat(fileno(fp), &sb);
    if (ret != 0) error(ioerror, "fstat did not return 0");
    if (sb.st_size > LONG_MAX)
        return LONG_MAX;
    sz = (long)sb.st_size;
    
    pos = ftell(fp);
    return sz - pos;
}

/* close the file,
   NULL the FILE*. */
void fileclose(mfile *mem,
               object f)
{
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
object fileread(mfile *mem,
                object f)
{
    if (!filestatus(mem, f)) error(ioerror, "fileread");
    return consint(fgetc(filefile(mem, f)));
}

/* if the file is valid,
   write a byte. */
void filewrite(mfile *mem,
               object f,
               object b)
{
    if (!filestatus(mem, f)) error(ioerror, "filewrite");
    if (fputc(b.int_.val, filefile(mem, f)) == EOF)
        error(ioerror, "filewrite");
}

