/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

#include <stdlib.h>
#include <stddef.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h"
#include "xpost_memory.h"  /* files store FILE*s in (local) mfile */
#include "xpost_object.h"  /* files are objects */
#include "xpost_context.h"

#include "xpost_error.h"  /* file functions may throw errors */
#include "xpost_file.h"  /* double-check prototypes */

#ifdef _WIN32
/*
 * FIXME: maybe use a WIN32 API for all this. See FIXME in xpost_op_file.c
 * Note:
 * this hack is needed as tmpfile in the Windows CRT opens
 * the temporary file in c:/ which needs administrator
 * privileges.
 */
static FILE *
f_tmpfile(void)
{
    char buf[XPOST_PATH_MAX];
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
    memset(buf, 0, l1 + l2 + 1);
    memcpy(buf, tmpdir, l1);
    memcpy(buf + l1, name, l2);
    //buf[l1 + l2] = '\0';

#ifdef DEBUG_FILE
    printf("fopen\n");
#endif
    return fopen(buf, "w+bD");
}
#else
# define f_tmpfile tmpfile
#endif

static int
disk_readch(Xpost_File *file)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    /*
     * FIXME: check if this work on Windows
     * indeed, on Windows, select() needs a socket, not a fd, and fileno() returns a fd
     * See http://stackoverflow.com/questions/6418232/how-to-use-select-to-read-input-from-keyboard-in-c/6419955#6419955
     * and https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499%28v=vs.85%29.aspx
     * Maybe WaitForSingleObject will also be needed
     */

#ifdef HAVE_SYS_SELECT_H
    {
        FILE *fp;
        fd_set reads, writes, excepts;
        int ret;
        struct timeval tv_timeout;
        //fp = xpost_file_get_file_pointer(ctx->lo, f);
        fp = df->file;
        FD_ZERO(&reads);
        FD_ZERO(&writes);
        FD_ZERO(&excepts);
        FD_SET(fileno(fp), &reads);
        tv_timeout.tv_sec = 0;
        tv_timeout.tv_usec = 0;

        ret = select(fileno(fp) + 1, &reads, &writes, &excepts, &tv_timeout);

        if (ret <= 0 || !FD_ISSET(fileno(fp), &reads))
        {
            /* byte not available, push retry, and request eval() to block this thread */
            //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "read", NULL,0,0));
            //xpost_stack_push(ctx->lo, ctx->os, f);
            //return ioblock;
            errno=EINTR;
            return EOF;
        }
    }
#endif

    return fgetc(df->file);
}

static int
disk_writech(Xpost_File *file, int c)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    return fputc(c, df->file);
}

static int
disk_close(Xpost_File *file)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;
    FILE *fp = df->file;
    int ret;

    if (fp == stdin || fp == stdout || fp == stderr) /* do NOT close standard files */
        return 0;
    ret = fclose(df->file);
    df->file = NULL;

    return ret;
}

static int
disk_flush(Xpost_File *file)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    return fflush(df->file);
}

static void
disk_purge(Xpost_File *file)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;
    xpost_fpurge(df->file);
}

static int
disk_unreadch(Xpost_File *file, int c)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    return ungetc(c, df->file);
}

static long
disk_tell(Xpost_File *file)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    return ftell(df->file);
}

static int
disk_seek(Xpost_File *file, long offset)
{
    Xpost_DiskFile *df = (Xpost_DiskFile*) file;

    return fseek(df->file, offset, SEEK_SET);
}

struct Xpost_File_Methods disk_methods =
{
    disk_readch,
    disk_writech,
    disk_close,
    disk_flush,
    disk_purge,
    disk_unreadch,
    disk_tell,
    disk_seek
};

static Xpost_File *
xpost_diskfile_open(const FILE *fp)
{
    Xpost_DiskFile *df = malloc(sizeof *df);

    if (df)
    {
        df->methods.methods = &disk_methods;
        df->file = (FILE*)fp;
    }

    return &df->methods;
}


static int
memory_readch(Xpost_File *f)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (!mf->is_read)
        return EOF;
    if (mf->read_next == mf->read_limit)
        return EOF;

    return mf->contents[ mf->read_next++ ];
}

static int
memory_writech(Xpost_File *f, int c)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (mf->is_read)
        return EOF;
    if (mf->write_next == mf->write_capacity){
        unsigned char *tmp;
        if (!mf->is_malloc)
	    return EOF;
	tmp = realloc(mf->contents, mf->write_capacity * 1.4 + 12);
	if (!tmp)
	    return EOF;
	mf->contents = tmp;
	mf->write_capacity = mf->write_capacity * 1.4 + 12;
    }

    mf->contents[ mf->write_next++ ] = c;
    return 0;
}

static int
memory_close(Xpost_File *f)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (mf->is_malloc)
        free(mf->contents);

    mf->contents = NULL;
    mf->read_next =
      mf->read_limit =
      mf->write_next =
      mf->write_capacity = 0;
    
    return 0;
}

static int
memory_flush(Xpost_File *f)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    (void)mf;
    return 0;
}

static void
memory_purge(Xpost_File *f)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (mf->is_read)
        mf->read_next = mf->read_limit;
}

static int
memory_unreadch(Xpost_File *f, int c)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (!mf->is_read)
        return EOF;
    if (mf->read_next <= 0)
        return EOF;

    mf->contents[ --mf->read_next ] = c;
    return 0;
}

static long
memory_tell(Xpost_File *f)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    return mf->read_next;
}

static int
memory_seek(Xpost_File *f, long pos)
{
    Xpost_MemoryFile *mf = (Xpost_MemoryFile *)f;

    if (pos > (ssize_t)mf->read_limit)
        return EOF;

    mf->read_next = pos;
    return 0;
}

struct Xpost_File_Methods memory_methods =
{
    memory_readch,
    memory_writech,
    memory_close,
    memory_flush,
    memory_purge,
    memory_unreadch,
    memory_tell,
    memory_seek
};

static Xpost_File *
xpost_memoryfile_open_read(unsigned char *ptr, size_t limit)
{
    Xpost_MemoryFile *mf = malloc(sizeof *mf);

    if (mf)
    {
        mf->methods.methods = &memory_methods;
        mf->contents = ptr;
        mf->is_read = 1;
        mf->is_malloc = 0;
        mf->read_next = 0;
        mf->read_limit = limit;
    }

    return &mf->methods;
}

static Xpost_File *
xpost_memoryfile_open_write(void)
{
    Xpost_MemoryFile *mf = malloc(sizeof *mf);

    if (mf)
    {
        mf->methods.methods = &memory_methods;
	mf->contents = NULL;
	mf->is_read = 0;
	mf->is_malloc = 1;
	mf->write_next = 0;
	mf->write_capacity = 0;
    }

    return &mf->methods;
}

/* filetype objects use a slightly different interpretation
   of the access field.
   It uses two flags rather than a 2-bit number.
   XPOST_OBJECT_TAG_ACCESS_FLAG_WRITE designates a writable file
   XPOST_OBJECT_TAG_ACCESS_FLAG_READ designates a readable file
   */

/* construct a file object.
   set the tag,
   use the "doubleword" field as a "pointer" (ent),
   allocate a Xpost_File,
   install the Xpost_File,
   return object.
   caller must set access for a readable file,
   default is writable.
   eg.
    FILE *fp = fopen(...);
    Xpost_Object f = readonly(xpost_file_cons(fp)).
 */
Xpost_Object xpost_file_cons(Xpost_Memory_File *mem,
                             /*@NULL@*/ const FILE *fp)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;
    Xpost_File *df;

#ifdef DEBUG_FILE
    printf("xpost_file_cons %p\n", fp);
#endif
    f.tag = filetype /*| (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET)*/;
    df = xpost_diskfile_open(fp);
    /* xpost_memory_table_alloc(mem, sizeof(FILE *), 0, &f.mark_.padw); */
    if (!xpost_memory_table_alloc(mem, sizeof df, filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
        return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof df, &df);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot save file pointer in VM");
        return invalid;
    }
    return f;
}

Xpost_Object xpost_file_cons_readbuffer(Xpost_Memory_File *mem,
					unsigned char *ptr,
					size_t limit)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;
    Xpost_File *mf;

    f.tag = filetype;
    mf = xpost_memoryfile_open_read(ptr, limit);
    if (!xpost_memory_table_alloc(mem, sizeof mf, filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
        return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof mf, &mf);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot save file pointer in VM");
        return invalid;
    }
    return f;
}

Xpost_Object xpost_file_cons_writebuffer(Xpost_Memory_File *mem)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;
    Xpost_File *mf;

    f.tag = filetype;
    mf = xpost_memoryfile_open_write();
    if (!xpost_memory_table_alloc(mem, sizeof mf, filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
	return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof mf, &mf);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot save file pointer in VM");
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
    if (fp == NULL)
    {
        XPOST_LOG_ERR("tmpfile() returned NULL");
        return ioerror;
    }
    while (c != EOF && c != '\n')
    {
        if (fputc(c, fp) == EOF) return ioerror;
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
    if (fp == NULL)
    {
        XPOST_LOG_ERR("tmpfile() returned NULL");
        return ioerror;
    }
    do
    {
        if (defer > -1)
        {
            if (defer > MAXNEST)
            {
                return syntaxerror;
            }
            switch(nest[defer])
            { /* what's the innermost nest? */
                case '{': /* within a proc, can end proc or begin proc, string, hex */
                    switch (c)
                    {
                        case '}': --defer; break;
                        case '{':
                        case '(':
                        case '<': nest[++defer] = c; break;
                    }
                    break;
                case '(': /* within a string, can begin or end nested paren */
                    switch (c)
                    {
                        case ')': --defer; break;
                        case '(': nest[++defer] = c; break;
                        case '\\': if (fputc(c, fp) == EOF) return ioerror;
                            c = fgetc(in);
                            if (c == EOF) goto done;
                            goto next;
                    }
                    break;
                case '<': /* hexstrings don't nest, can only end it */
                    if (c == '>') --defer;
                    break;
            }
        }
        else
            switch (c)
            { /* undefined, can begin any structure */
                case '{':
                case '(':
                case '<': nest[++defer] = c; break;
                case '\\': if (fputc(c, fp) == EOF) return ioerror;
                    c = fgetc(in); break;
            }
        if (c == '\n')
        {
            if (defer == -1) goto done;
            { /* sub-prompt */
                int i;
                for (i = 0; i <= defer; i++)
                    putchar(nest[i]);
                fputs(".:", stdout);
                fflush(NULL);
            }
        }
next:
        if (fputc(c, fp) == EOF) return ioerror;
        c = fgetc(in);
    } while(c != EOF);
done:
    fseek(fp, 0, SEEK_SET);
    //return fp;
    *out = fp;
    return 0;
}

/* Open a file object,
   check for "special" filenames,
   fallback to fopen. */
int xpost_file_open(Xpost_Memory_File *mem,
                    char *fn,
                    char *mode,
                    Xpost_Object *retval)
{
    Xpost_Object f;
    FILE *fp;
    int ret;

    f.tag = filetype;

    if (strcmp(fn, "%stdin") == 0)
    {
        if (strcmp(mode, "r") != 0)
        {
            return invalidfileaccess;
        }
        f = xpost_file_cons(mem, stdin);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    }
    else if (strcmp(fn, "%stdout") == 0)
    {
        if (strcmp(mode, "w") != 0)
        {
            return invalidfileaccess;
        }
        f = xpost_file_cons(mem, stdout);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_WRITE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    }
    else if (strcmp(fn, "%stderr") == 0)
    {
        if (strcmp(mode, "w") != 0)
        {
            return invalidfileaccess;
        }
        f = xpost_file_cons(mem, stderr);
    }
    else if (strcmp(fn, "%lineedit") == 0)
    {
        ret = lineedit(stdin, &fp);
        if (ret)
        {
            return ret;
        }
        f = xpost_file_cons(mem, fp);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    }
    else if (strcmp(fn, "%statementedit") == 0)
    {
        ret = statementedit(stdin, &fp);
        if (ret)
        {
            return ret;
        }
        f = xpost_file_cons(mem, fp);
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    }
    else
    {
#ifdef DEBUG_FILE
        printf("fopen\n");
#endif
        fp = fopen(fn, mode);
        if (fp == NULL)
        {
            switch (errno)
            {
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
        f = xpost_file_cons(mem, fp);
        if (strcmp(mode, "r") == 0)
        {
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_READ << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        }
        else if (strcmp(mode, "r+") == 0)
        {
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= ( (XPOST_OBJECT_TAG_ACCESS_FILE_READ
                    | XPOST_OBJECT_TAG_ACCESS_FILE_WRITE)
                    << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        }
        else if (strcmp(mode, "w") == 0)
        {
            f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
            f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_WRITE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
        }
        else
        {
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
Xpost_File *xpost_file_get_file_pointer(Xpost_Memory_File *mem,
                                        Xpost_Object f)
{
    Xpost_File *fp;
    int ret;

    ret = xpost_memory_get(mem, f.mark_.padw, 0, sizeof fp, &fp);
    if (!ret)
    {
        return NULL;
    }
    return fp;
}

/* make sure the FILE* is not null */
int xpost_file_get_status(Xpost_Memory_File *mem,
                          Xpost_Object f)
{
    return xpost_file_get_file_pointer(mem, f) != NULL;
}

//FIXME assumes DiskFile subtype
/* call fstat. */
int xpost_file_get_bytes_available(Xpost_Memory_File *mem,
                                   Xpost_Object f,
                                   int *retval)
{
    int ret;
    FILE *fp;
    struct stat sb;
    long sz, pos;

    fp = ((Xpost_DiskFile*)xpost_file_get_file_pointer(mem, f))->file;
    if (!fp) return ioerror;
    ret = fstat(fileno(fp), &sb);
    if (ret != 0)
    {
        XPOST_LOG_ERR("fstat did not return 0");
        return ioerror;
    }
    if (sb.st_size > LONG_MAX)
        return rangecheck;
    sz = (long)sb.st_size;

    pos = ftell(fp);
    if ((sz - pos) > INT_MAX)
        return rangecheck;

    *retval = (int)(sz - pos);

    return 0;
}

/* close the file,
   NULL the FILE*. */
int xpost_file_object_close(Xpost_Memory_File *mem,
                            Xpost_Object f)
{
    Xpost_File *fp;
    int ret;

    fp = xpost_file_get_file_pointer(mem, f);
    if (fp)
    {
#ifdef DEBUG_FILE
        printf("fclose");
#endif

        xpost_file_close(fp);
        fp = NULL;
        ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof fp, &fp);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot write NULL over FILE* in VM");
            return VMerror;
        }
    }
    return 0;
}

// returned value is count of complete size-sized chunks read.
// function may have read up to size-1 additional bytes.
int xpost_file_read(char *buf, int size, int count, Xpost_File *fp)
{
    int c, i, j, k = 0;

    for (i = 0; i < count; ++i)
    {
        for (j = 0; j < size; ++j)
	{
            c = xpost_file_getc(fp);
	    if (c == EOF) return i;
            buf[k++] = c;
	}
    }

    return i;
}

int xpost_file_write(const char *buf, int size, int count, Xpost_File *fp)
{
    int i, j, k = 0;

    for (i = 0; i < count; ++i)
        for (j = 0; j < size; ++j)
            if (xpost_file_putc(fp, buf[k++]) == EOF) return i;

    return i;
}

/* if the file is valid,
   read a byte. */
Xpost_Object xpost_file_read_byte(Xpost_Memory_File *mem,
                                  Xpost_Object f)
{
    int c;

    if (!xpost_file_get_status(mem, f))
    {
        return invalid;
    }
retry:
    errno=0;
    c = xpost_file_getc(xpost_file_get_file_pointer(mem, f));
    if (c == EOF && errno==EINTR)
        goto retry;

    return xpost_int_cons(c);
}

/* if the file is valid,
   write a byte. */
int xpost_file_write_byte(Xpost_Memory_File *mem,
                          Xpost_Object f,
                          Xpost_Object b)
{
    if (!xpost_file_get_status(mem, f))
    {
        return ioerror;
    }
    if (xpost_file_putc(xpost_file_get_file_pointer(mem, f), b.int_.val) == EOF)
    {
        return ioerror;
    }
    return 0;
}
