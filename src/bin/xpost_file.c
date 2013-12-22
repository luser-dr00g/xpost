/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "xpost_log.h"
#include "xpost_memory.h"  /* files store FILE*s in (local) mfile */
#include "xpost_object.h"  /* files are objects */
#include "xpost_context.h"

#include "xpost_interpreter.h"  /* interpreter */
#include "xpost_error.h"  /* file functions may throw errors */
#include "xpost_file.h"  /* double-check prototypes */

#ifdef __MINGW32__
/*
 * Note:
 * this hack is needed as tmpfile in the Windows CRT opens
 * the temporary file in c:/ which needs administrator
 * privileges.
 */
static FILE *
f_tmpfile(void)
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

#ifdef DEBUG_FILE
  printf("fopen\n");
#endif
  return fopen(buf, "w+bD");
}
#else
# define f_tmpfile tmpfile
#endif


/* filetype objects use a slightly different interpretation
   of the access field.
   It uses two flags rather than a 2-bit number.
   XPOST_OBJECT_TAG_ACCESS_FLAG_WRITE designates a writable file
   XPOST_OBJECT_TAG_ACCESS_FLAG_READ designates a readable file
   */

/* construct a file object.
   set the tag,
   use the "doubleword" field as a "pointer" (ent),
   allocate a FILE *,
   install the FILE *,
   return object.
   caller must set access for a readable file,
   default is writable.
   eg.
    FILE *fp = fopen(...);
    Xpost_Object f = readonly(consfile(fp)).
 */
Xpost_Object consfile(Xpost_Memory_File *mem,
        /*@NULL@*/ FILE *fp)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;

#ifdef DEBUG_FILE
    printf("consfile %p\n", fp);
#endif
    f.tag = filetype /*| (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET)*/;
    /* xpost_memory_table_alloc(mem, sizeof(FILE *), 0, &f.mark_.padw); */
    if (!xpost_memory_table_alloc(mem, sizeof(FILE *), filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
        return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot save FILE* in VM");
        return invalid;
    }
    return f;
}

/* pinch-off a tmpfile containing one line from file. */
/*@null@*/
static
int lineedit(FILE *in, FILE **out)
{
    FILE *fp;
    int c;

    c = fgetc(in);
    if (c == EOF)
    {
        return undefinedfilename;
    }
#ifdef DEBUG_FILE
	printf("tmpfile (fdopen)\n");
#endif
    fp = f_tmpfile();
    if (fp == NULL) {
        XPOST_LOG_ERR("tmpfile() returned NULL");
        return ioerror;
    }
    while (c != EOF && c != '\n') {
        (void)fputc(c, fp);
        c = fgetc(in);
    }
    fseek(fp, 0, SEEK_SET);
    //return fp;
    *out = fp;
    return 0;
}

enum { MAXNEST = 20 };

/* pinch-off a tmpfile containing one "statement" from file. */
/*@null@*/
static
int statementedit(FILE *in, FILE **out)
{
    FILE *fp;
    int c;
    char nest[MAXNEST] = {0}; /* any of {(< waiting for matching >)} */
    int defer = -1; /* defer is a flag (-1 == false)
                       and an index into nest[] */

    c = fgetc(in);
    if (c == EOF)
    {
        return undefinedfilename;
    }
#ifdef DEBUG_FILE
	printf("tmpfile (fdopen)\n");
#endif
    fp = f_tmpfile();
    if (fp == NULL) {
        XPOST_LOG_ERR("tmpfile() returned NULL");
        return ioerror;
    }
    do {
        if (defer > -1) {
            if (defer > MAXNEST)
            {
                return syntaxerror;
            }
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
    //return fp;
    *out = fp;
    return 0;
}

/* check for "special" filenames,
   fallback to fopen. */
int fileopen(Xpost_Memory_File *mem,
        char *fn,
        char *mode,
        Xpost_Object *retval)
{
    Xpost_Object f;
    FILE *fp;
    int ret;

    f.tag = filetype;

    if (strcmp(fn, "%stdin")==0) {
        if (strcmp(mode, "r")!=0)
        {
            return invalidfileaccess;
        }
        f = consfile(mem, stdin);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    } else if (strcmp(fn, "%stdout")==0) {
        if (strcmp(mode, "w")!=0)
        {
            return invalidfileaccess;
        }
        f = consfile(mem, stdout);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_WRITE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    } else if (strcmp(fn, "%stderr")==0) {
        if (strcmp(mode, "w")!=0)
        {
            return invalidfileaccess;
        }
        f = consfile(mem, stderr);
    } else if (strcmp(fn, "%lineedit")==0) {
        ret = lineedit(stdin, &fp);
        if (ret)
        {
            return ret;
        }
        f = consfile(mem, fp);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    } else if (strcmp(fn, "%statementedit")==0) {
        ret = statementedit(stdin, &fp);
        if (ret)
        {
            return ret;
        }
        f = consfile(mem, fp);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    } else {
#ifdef DEBUG_FILE
		printf("fopen\n");
#endif
        fp = fopen(fn, mode);
        if (fp == NULL) {
            switch (errno) {
            case EACCES:
                return invalidfileaccess;
                break;
            case ENOENT:
                return undefinedfilename;
                break;
            default:
                return unregistered;
                break;
            }
        }
        f = consfile(mem, fp);
        if (strcmp(mode, "r")==0){
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        } else if (strcmp(mode, "r+")==0){
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= ( (XPOST_OBJECT_TAG_ACCESS_FILE_READ
                    | XPOST_OBJECT_TAG_ACCESS_FILE_WRITE)
                    << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        } else if (strcmp(mode, "w")==0){
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_WRITE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        } else {
            XPOST_LOG_ERR("bad mode string");
            return ioerror;
        }

    }

    f.tag |= XPOST_OBJECT_TAG_DATA_FLAG_LIT;
    //return f;
    *retval = f;
    return 0;
}

/* adapter:
           FILE* <- filetype object
   yield the FILE* from a filetype object */
FILE *filefile(Xpost_Memory_File *mem,
               Xpost_Object f)
{
    FILE *fp;
    int ret;

    ret = xpost_memory_get(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
    if (!ret)
    {
        return NULL;
    }
    return fp;
}

/* make sure the FILE* is not null */
int filestatus(Xpost_Memory_File *mem,
                Xpost_Object f)
{
    return filefile(mem, f) != NULL;
}

/* call fstat. */
int filebytesavailable(Xpost_Memory_File *mem,
                        Xpost_Object f,
                        long *retval)
{
    int ret;
    FILE *fp;
    struct stat sb;
    long sz, pos;

    fp = filefile(mem, f);
    if (!fp) return -1;
    ret = fstat(fileno(fp), &sb);
    if (ret != 0)
    {
        XPOST_LOG_ERR("fstat did not return 0");
        return ioerror;
    }
    if (sb.st_size > LONG_MAX)
        return LONG_MAX;
    sz = (long)sb.st_size;
    
    pos = ftell(fp);
    //return sz - pos;
    *retval = sz - pos;
    return 0;
}

/* close the file,
   NULL the FILE*. */
int fileclose(Xpost_Memory_File *mem,
               Xpost_Object f)
{
    FILE *fp;
    int ret;

    fp = filefile(mem, f);
    if (fp) {
#ifdef DEBUG_FILE
		printf("fclose");
#endif
        if (fp == stdin || fp == stdout || fp == stderr) /* do NOT close standard files */
            return 0;

        fclose(fp);
        fp = NULL;
        ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof(FILE *), &fp);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot write NULL over FILE* in VM");
            return VMerror;
        }
    }
    return 0;
}

/* if the file is valid,
   read a byte. */
Xpost_Object fileread(Xpost_Memory_File *mem,
                Xpost_Object f)
{
    if (!filestatus(mem, f))
    {
        //error(ioerror, "fileread");
        return invalid;
    }
    return xpost_cons_int(fgetc(filefile(mem, f)));
}

/* if the file is valid,
   write a byte. */
int filewrite(Xpost_Memory_File *mem,
               Xpost_Object f,
               Xpost_Object b)
{
    if (!filestatus(mem, f))
    {
        //error(ioerror, "filewrite");
        return ioerror;
    }
    if (fputc(b.int_.val, filefile(mem, f)) == EOF)
    {
        //error(ioerror, "filewrite");
        return ioerror;
    }
    return 0;
}

