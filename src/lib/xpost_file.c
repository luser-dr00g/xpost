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
#include <ctype.h>

#ifdef HAVE_ZLIB
# include <zlib.h>
#endif

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

/* --- file-access sandbox -------------------------------------------------
   A process-wide, one-way latch. Before engaging, disk access is
   unrestricted; once engaged, an open by the running program is denied
   unless the path resolves within a permitted directory. This is defence
   in depth around the operating-system confinement of the host process,
   not a substitute for it. Resource-file loading is separately confined
   (see xpost_diskfile_fopen_beneath) and does not consult this. */

#define XPOST_PATH_PERMIT_MAX 64

static char *xpost_permit_read_dir[XPOST_PATH_PERMIT_MAX];
static int xpost_permit_read_cnt = 0;
static char *xpost_permit_write_dir[XPOST_PATH_PERMIT_MAX];
static int xpost_permit_write_cnt = 0;
static int xpost_path_control_engaged = 0;

static int
xpost_path_permit_add(char **tab, int *cnt, const char *dir)
{
    char *rp;

    if (xpost_path_control_engaged) /* the permit set is frozen once engaged */
        return 0;
    if (*cnt >= XPOST_PATH_PERMIT_MAX)
        return 0;
    rp = xpost_realpath(dir);
    if (!rp)
        return 0;
    tab[*cnt] = rp;
    ++*cnt;
    return 1;
}

int
xpost_path_permit_read(const char *dir)
{
    return xpost_path_permit_add(xpost_permit_read_dir,
                                 &xpost_permit_read_cnt, dir);
}

int
xpost_path_permit_write(const char *dir)
{
    return xpost_path_permit_add(xpost_permit_write_dir,
                                 &xpost_permit_write_cnt, dir);
}

void
xpost_path_control_engage(void)
{
    xpost_path_control_engaged = 1;
}

/* does canon sit within one of the cnt permitted directories? */
static int
xpost_path_within(const char *canon, char *const *tab, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++)
    {
        size_t rl = strlen(tab[i]);

#ifdef _WIN32
        /* Windows paths are case-insensitive and GetFullPathName yields
           backslash separators (mirrors the beneath-root check) */
        if (_strnicmp(canon, tab[i], rl) == 0 &&
            (canon[rl] == '\\' || canon[rl] == '/' || canon[rl] == '\0'))
            return 1;
#else
        if (strncmp(canon, tab[i], rl) == 0 &&
            (canon[rl] == '/' || canon[rl] == '\0'))
            return 1;
#endif
    }
    return 0;
}

/* Is opening `path` (for writing when `write`) permitted? Resolves the
   path -- or, for a not-yet-existent write target, its parent directory
   with the leaf reattached -- and checks it against the permit list. */
static int
xpost_path_permitted(const char *path, int write)
{
    char *canon = xpost_realpath(path);
    int ok;

    if (canon)
    {
        ok = write
             ? xpost_path_within(canon, xpost_permit_write_dir, xpost_permit_write_cnt)
             : xpost_path_within(canon, xpost_permit_read_dir, xpost_permit_read_cnt);
        free(canon);
        return ok;
    }

    /* the path does not resolve: only a create (write) is meaningful */
    if (!write)
        return 0;
    {
        char buf[XPOST_PATH_MAX];
        char full[XPOST_PATH_MAX];
        char *slash;
        char *cdir;
        const char *parent;
        const char *base;

        if (strlen(path) >= sizeof buf)
            return 0;
        strcpy(buf, path);
        slash = strrchr(buf, '/');
        if (slash)
        {
            *slash = '\0';
            parent = buf[0] ? buf : "/";
            base = slash + 1;
        }
        else
        {
            parent = ".";
            base = buf;
        }
        cdir = xpost_realpath(parent);
        if (!cdir)
            return 0;
        ok = (snprintf(full, sizeof full, "%s/%s", cdir, base) < (int)sizeof full) &&
             xpost_path_within(full, xpost_permit_write_dir, xpost_permit_write_cnt);
        free(cdir);
        return ok;
    }
}

/* The single path-to-stream opener for disk-backed files: every disk file
   the interpreter opens is created here, so file-access policy has one
   enforcement point. internal marks a trusted interpreter-managed path
   (temporary scratch) rather than one derived from the running program.
   Access policy is not yet applied; the parameter fixes the call sites so
   that only this function changes when it is. */
FILE *
xpost_diskfile_fopen(const char *path, const char *mode, int internal, int *err)
{
    FILE *fp;

    /* a program-driven open under the engaged sandbox must lie within a
       permitted directory; trusted interpreter-managed opens are exempt */
    if (!internal && xpost_path_control_engaged)
    {
        int write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');

        if (!xpost_path_permitted(path, write))
        {
            *err = invalidfileaccess;
            return NULL;
        }
    }

    fp = fopen(path, mode);
    if (!fp)
    {
        switch (errno)
        {
            case EACCES: *err = invalidfileaccess; break;
            case ENOENT: *err = undefinedfilename; break;
            default:     *err = unregistered; break;
        }
        return NULL;
    }
    *err = 0;
    return fp;
}

/* Is s[0..len) a safe single path component ("leaf")? Externally-derived
   resource names are validated with this so they cannot express a path:
   rejected are separators of either platform, ':' (drive letter / NTFS
   stream), NUL and other control bytes, '.' and '..', a leading dot or
   space, a trailing dot or space (which Windows strips), and the reserved
   Windows device names. Bytes are otherwise restricted to [A-Za-z0-9._-].
   Returns 1 if safe, 0 otherwise. */
int
xpost_path_safe_leaf(const char *s, size_t len)
{
    static const char *const reserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    size_t i;
    size_t stem;
    size_t r;

    if (len == 0)
        return 0;
    if (s[0] == '.' || s[0] == ' ')                 /* leading dot ('.', '..',
                                                       hidden) or space */
        return 0;
    if (s[len - 1] == '.' || s[len - 1] == ' ')     /* trailing dot or space */
        return 0;
    for (i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)s[i];

        if (c < 0x20 || c == 0x7f)                  /* NUL and control bytes */
            return 0;
        if (c == '/' || c == '\\' || c == ':')      /* separators, drive/ADS */
            return 0;
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-'))
            return 0;
    }
    /* reserved device name: the stem before the first '.', case-insensitive */
    for (stem = 0; stem < len && s[stem] != '.'; stem++)
        ;
    for (r = 0; r < sizeof reserved / sizeof reserved[0]; r++)
    {
        size_t rl = strlen(reserved[r]);
        size_t k;

        if (rl != stem)
            continue;
        for (k = 0; k < rl; k++)
        {
            unsigned char c = (unsigned char)s[k];

            if (c >= 'a' && c <= 'z')
                c = (unsigned char)(c - 32);
            if (c != (unsigned char)reserved[r][k])
                break;
        }
        if (k == rl)
            return 0;
    }
    return 1;
}

/* Open rel for reading beneath root, with the OS confining resolution to
   root (see xpost_open_beneath), mapping the failure to an error code.
   rel should already be composed of xpost_path_safe_leaf components. */
FILE *
xpost_diskfile_fopen_beneath(const char *root, const char *rel, int *err)
{
    FILE *fp = xpost_open_beneath(root, rel);

    if (!fp)
    {
        switch (errno)
        {
            case EACCES:
            case EPERM:
#ifdef ELOOP
            case ELOOP:
#endif
#ifdef EXDEV
            case EXDEV:
#endif
                *err = invalidfileaccess;
                break;
            case ENOENT:
            case ENOTDIR:
                *err = undefinedfilename;
                break;
#ifdef ENAMETOOLONG
            case ENAMETOOLONG:
                *err = limitcheck;
                break;
#endif
            default:
                *err = unregistered;
                break;
        }
        return NULL;
    }
    *err = 0;
    return fp;
}

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
    {
        int err;
        return xpost_diskfile_fopen(buf, "w+bD", 1, &err);
    }
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
    if (df->poll_before_read)
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

    /* the interpreter is single-threaded, so the unlocked fast path is safe;
       unistd.h is present on mingw but does not declare getc_unlocked there */
#if defined(_WIN32)
    return _getc_nolock(df->file);
#elif defined(HAVE_UNISTD_H)
    return getc_unlocked(df->file);
#else
    return fgetc(df->file);
#endif
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
        struct stat st;

        df->methods.methods = &disk_methods;
        df->file = (FILE*)fp;
        /* reads from a regular file never block, so only poll fds that
           can stall (pipes, terminals, sockets) */
        df->poll_before_read = !(fstat(fileno(df->file), &st) == 0 &&
                                 S_ISREG(st.st_mode));
    }

    return &df->methods;
}


/* the underlying stdio stream of a disk-backed file, or NULL */
FILE *xpost_file_stdio_stream_get(Xpost_File *f)
{
    if (f && f->methods == &disk_methods)
        return ((Xpost_DiskFile *)f)->file;
    return NULL;
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

/* ASCII85Decode filter: a read file decoding an ASCII base-85 stream
   from an underlying file. Whitespace between coded characters is
   ignored (a stream's layout carries no data), 'z' abbreviates four
   zero bytes, and the "~>" marker ends the data with the underlying
   file positioned just after it, so a program executing
   "currentfile /ASCII85Decode filter cvx exec" resumes cleanly at
   end of data. The source file is not owned: closing the filter
   leaves it open. */
typedef struct Xpost_FilterFile
{
    Xpost_File methods;
    Xpost_File *source;
    unsigned char out[4];
    int outn, outi;    /* decoded bytes pending */
    int pushback;      /* one unread byte, or -1 */
    int eod;
    long count;        /* decoded bytes delivered (tell) */
} Xpost_FilterFile;

static int
a85_readch(Xpost_File *f)
{
    Xpost_FilterFile *ff = (Xpost_FilterFile *)f;
    unsigned int grp[5];
    int n, c;

    if (ff->pushback >= 0)
    {
        c = ff->pushback;
        ff->pushback = -1;
        return c;
    }
    if (ff->outi < ff->outn)
    {
        ff->count++;
        return ff->out[ff->outi++];
    }
    if (ff->eod)
        return EOF;

    /* gather the next coded group */
    n = 0;
    for (;;)
    {
        c = xpost_file_getc(ff->source);
        if (c == EOF)
        {
            ff->eod = 1;
            break;
        }
        if (isspace(c))
            continue;
        if (c == '~')
        {
            /* end of data: consume the closing '>' */
            do
            {
                c = xpost_file_getc(ff->source);
            } while (c != EOF && isspace(c));
            if (c != '>' && c != EOF)
                xpost_file_ungetc(ff->source, c);
            ff->eod = 1;
            break;
        }
        if (c == 'z' && n == 0)
        {
            ff->out[0] = ff->out[1] = ff->out[2] = ff->out[3] = 0;
            ff->outn = 4;
            ff->outi = 1;
            ff->count++;
            return 0;
        }
        if (c < '!' || c > 'u')
        {
            XPOST_LOG_ERR("character %d in ASCII85Decode stream", c);
            ff->eod = 1;
            break;
        }
        grp[n++] = c - '!';
        if (n == 5)
            break;
    }

    if (n <= 1)   /* nothing, or a dangling single character */
        return EOF;

    {
        unsigned int tuple = 0;
        int i, nbytes = n - 1;

        for (i = 0; i < 5; i++)
            tuple = tuple * 85 + (i < n ? grp[i] : 84);  /* pad with 'u' */
        ff->out[0] = (tuple >> 24) & 0xff;
        ff->out[1] = (tuple >> 16) & 0xff;
        ff->out[2] = (tuple >> 8) & 0xff;
        ff->out[3] = tuple & 0xff;
        ff->outn = nbytes;
        ff->outi = 1;
        ff->count++;
        return ff->out[0];
    }
}

static int
a85_writech(Xpost_File *f, int c)
{
    (void)f;
    (void)c;
    return EOF;
}

static int
a85_close(Xpost_File *f)
{
    Xpost_FilterFile *ff = (Xpost_FilterFile *)f;

    /* the source stays open; just stop producing */
    ff->eod = 1;
    ff->outi = ff->outn = 0;
    ff->pushback = -1;
    return 0;
}

static int
a85_flush(Xpost_File *f)
{
    (void)f;
    return 0;
}

static void
a85_purge(Xpost_File *f)
{
    Xpost_FilterFile *ff = (Xpost_FilterFile *)f;

    ff->eod = 1;
    ff->outi = ff->outn = 0;
    ff->pushback = -1;
}

static int
a85_unreadch(Xpost_File *f, int c)
{
    Xpost_FilterFile *ff = (Xpost_FilterFile *)f;

    if (ff->pushback >= 0)
        return EOF;
    ff->pushback = c;
    return 0;
}

static long
a85_tell(Xpost_File *f)
{
    Xpost_FilterFile *ff = (Xpost_FilterFile *)f;

    return ff->count;
}

static int
a85_seek(Xpost_File *f, long offset)
{
    (void)f;
    (void)offset;
    return -1;
}

struct Xpost_File_Methods a85_methods =
{
    a85_readch,
    a85_writech,
    a85_close,
    a85_flush,
    a85_purge,
    a85_unreadch,
    a85_tell,
    a85_seek
};

static Xpost_File *
xpost_filterfile_open_a85(Xpost_File *source)
{
    Xpost_FilterFile *ff = malloc(sizeof *ff);

    if (ff)
    {
        ff->methods.methods = &a85_methods;
        ff->source = source;
        ff->outn = ff->outi = 0;
        ff->pushback = -1;
        ff->eod = 0;
        ff->count = 0;
    }

    return &ff->methods;
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
    FILE *fp = xpost_diskfile_fopen(path, mode, 0, &err);
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

/* ASCIIHexDecode filter: hexadecimal digit pairs to bytes, whitespace
   ignored, '>' ends the data (an odd final digit is padded with 0). */
typedef struct Xpost_HexFile
{
    Xpost_File methods;
    Xpost_File *source;
    int pushback;
    int eod;
} Xpost_HexFile;

static int
_hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int
hex_readch(Xpost_File *f)
{
    Xpost_HexFile *ff = (Xpost_HexFile *)f;
    int c, hi, lo;

    if (ff->pushback >= 0)
    {
        c = ff->pushback;
        ff->pushback = -1;
        return c;
    }
    if (ff->eod)
        return EOF;
    do
    {
        c = xpost_file_getc(ff->source);
    } while (c != EOF && isspace(c));
    if (c == EOF || c == '>')
    {
        ff->eod = 1;
        return EOF;
    }
    hi = _hexval(c);
    if (hi < 0)
    {
        XPOST_LOG_ERR("character %d in ASCIIHexDecode stream", c);
        ff->eod = 1;
        return EOF;
    }
    do
    {
        c = xpost_file_getc(ff->source);
    } while (c != EOF && isspace(c));
    if (c == EOF || c == '>')
    {
        ff->eod = 1;
        lo = 0;
    }
    else
    {
        lo = _hexval(c);
        if (lo < 0)
        {
            XPOST_LOG_ERR("character %d in ASCIIHexDecode stream", c);
            ff->eod = 1;
            lo = 0;
        }
    }
    return (hi << 4) | lo;
}

/* RunLengthDecode filter: a length byte 0..127 copies that many plus
   one literal bytes; 129..255 repeats the next byte 257 minus the
   length times; 128 ends the data. */
typedef struct Xpost_RleFile
{
    Xpost_File methods;
    Xpost_File *source;
    int pushback;
    int eod;
    int litrun;   /* literal bytes still to copy */
    int reprun;   /* repetitions still to emit */
    int repbyte;
} Xpost_RleFile;

static int
rle_readch(Xpost_File *f)
{
    Xpost_RleFile *ff = (Xpost_RleFile *)f;
    int c;

    if (ff->pushback >= 0)
    {
        c = ff->pushback;
        ff->pushback = -1;
        return c;
    }
    if (ff->reprun > 0)
    {
        ff->reprun--;
        return ff->repbyte;
    }
    if (ff->litrun > 0)
    {
        ff->litrun--;
        c = xpost_file_getc(ff->source);
        if (c == EOF)
            ff->eod = 1;
        return c;
    }
    if (ff->eod)
        return EOF;
    c = xpost_file_getc(ff->source);
    if (c == EOF || c == 128)
    {
        ff->eod = 1;
        return EOF;
    }
    if (c < 128)
    {
        ff->litrun = c;   /* this byte plus litrun more */
        c = xpost_file_getc(ff->source);
        if (c == EOF)
            ff->eod = 1;
        return c;
    }
    ff->repbyte = xpost_file_getc(ff->source);
    if (ff->repbyte == EOF)
    {
        ff->eod = 1;
        return EOF;
    }
    ff->reprun = 257 - c - 1;   /* this byte plus reprun more */
    return ff->repbyte;
}

/* SubFileDecode filter: pass bytes through until the EOD string has
   been seen count times; the final occurrence is consumed but not
   delivered, leaving the source just after it. A count of zero with
   a non-empty string ends at the first occurrence; an empty string
   makes count a plain byte count. */
typedef struct Xpost_SubFile
{
    Xpost_File methods;
    Xpost_File *source;
    int pushback;
    int eod;
    int count;
    unsigned char eodstr[64];
    int eodlen;
    unsigned char pend[64];   /* partially matched prefix to re-emit */
    int pendn, pendi;
} Xpost_SubFile;

static int
subfile_readch(Xpost_File *f)
{
    Xpost_SubFile *ff = (Xpost_SubFile *)f;
    int c;
    int matched;

    if (ff->pushback >= 0)
    {
        c = ff->pushback;
        ff->pushback = -1;
        return c;
    }
    if (ff->pendi < ff->pendn)
        return ff->pend[ff->pendi++];
    if (ff->eod)
        return EOF;

    if (ff->eodlen == 0)
    {
        /* byte count mode */
        if (ff->count <= 0)
        {
            ff->eod = 1;
            return EOF;
        }
        ff->count--;
        c = xpost_file_getc(ff->source);
        if (c == EOF)
            ff->eod = 1;
        return c;
    }

    /* match the EOD string; on a partial mismatch the consumed prefix
       replays from the pending buffer (the string may not contain a
       repeated prefix hazard longer than itself, so rescanning from
       the second byte is not needed for the delimiters in use) */
    matched = 0;
    for (;;)
    {
        c = xpost_file_getc(ff->source);
        if (c == EOF)
        {
            ff->eod = 1;
            if (matched)
            {
                memcpy(ff->pend, ff->eodstr, matched);
                ff->pendn = matched;
                ff->pendi = 1;
                return ff->pend[0];
            }
            return EOF;
        }
        if ((unsigned char)c == ff->eodstr[matched])
        {
            matched++;
            if (matched == ff->eodlen)
            {
                if (ff->count > 1)
                {
                    /* deliver this occurrence and keep going */
                    ff->count--;
                    memcpy(ff->pend, ff->eodstr, matched);
                    ff->pendn = matched;
                    ff->pendi = 1;
                    return ff->pend[0];
                }
                ff->eod = 1;
                return EOF;
            }
            continue;
        }
        if (matched)
        {
            /* replay the matched prefix, then this byte */
            memcpy(ff->pend, ff->eodstr, matched);
            ff->pend[matched] = (unsigned char)c;
            ff->pendn = matched + 1;
            ff->pendi = 1;
            return ff->pend[0];
        }
        return c;
    }
}

#ifdef HAVE_ZLIB
/* FlateDecode filter: a zlib stream inflated from the source, which
   is left positioned just after the compressed data. */
typedef struct Xpost_FlateFile
{
    Xpost_File methods;
    Xpost_File *source;
    int pushback;
    int eod;
    z_stream strm;
    unsigned char in[1];
    unsigned char out[4096];
    int outn, outi;
} Xpost_FlateFile;

static int
flate_readch(Xpost_File *f)
{
    Xpost_FlateFile *ff = (Xpost_FlateFile *)f;
    int c, ret;

    if (ff->pushback >= 0)
    {
        c = ff->pushback;
        ff->pushback = -1;
        return c;
    }
    if (ff->outi < ff->outn)
        return ff->out[ff->outi++];
    if (ff->eod)
        return EOF;

    ff->strm.next_out = ff->out;
    ff->strm.avail_out = sizeof(ff->out);
    while (ff->strm.avail_out == sizeof(ff->out))
    {
        if (ff->strm.avail_in == 0)
        {
            c = xpost_file_getc(ff->source);
            if (c == EOF)
            {
                ff->eod = 1;
                break;
            }
            ff->in[0] = (unsigned char)c;
            ff->strm.next_in = ff->in;
            ff->strm.avail_in = 1;
        }
        ret = inflate(&ff->strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END)
        {
            ff->eod = 1;
            break;
        }
        if (ret != Z_OK && ret != Z_BUF_ERROR)
        {
            XPOST_LOG_ERR("FlateDecode error %d", ret);
            ff->eod = 1;
            break;
        }
    }
    ff->outn = (int)(sizeof(ff->out) - ff->strm.avail_out);
    ff->outi = 0;
    if (ff->outi < ff->outn)
    {
        ff->outi = 1;
        return ff->out[0];
    }
    return EOF;
}
#endif

/* method boilerplate shared by the decode filters: they are read-only
   streams over an unowned source with one byte of pushback */
typedef struct Xpost_FilterBase
{
    Xpost_File methods;
    Xpost_File *source;
    int pushback;
    int eod;
} Xpost_FilterBase;

static int
filter_writech(Xpost_File *f, int c)
{
    (void)f;
    (void)c;
    return EOF;
}

static int
filter_close(Xpost_File *f)
{
    Xpost_FilterBase *ff = (Xpost_FilterBase *)f;

    ff->eod = 1;
    ff->pushback = -1;
    return 0;
}

static int
filter_flush(Xpost_File *f)
{
    (void)f;
    return 0;
}

static void
filter_purge(Xpost_File *f)
{
    Xpost_FilterBase *ff = (Xpost_FilterBase *)f;

    ff->eod = 1;
    ff->pushback = -1;
}

static int
filter_unreadch(Xpost_File *f, int c)
{
    Xpost_FilterBase *ff = (Xpost_FilterBase *)f;

    if (ff->pushback >= 0)
        return EOF;
    ff->pushback = c;
    return 0;
}

static long
filter_tell(Xpost_File *f)
{
    (void)f;
    return 0;
}

static int
filter_seek(Xpost_File *f, long offset)
{
    (void)f;
    (void)offset;
    return -1;
}

struct Xpost_File_Methods hex_methods =
{
    hex_readch, filter_writech, filter_close, filter_flush,
    filter_purge, filter_unreadch, filter_tell, filter_seek
};

struct Xpost_File_Methods rle_methods =
{
    rle_readch, filter_writech, filter_close, filter_flush,
    filter_purge, filter_unreadch, filter_tell, filter_seek
};

struct Xpost_File_Methods subfile_methods =
{
    subfile_readch, filter_writech, filter_close, filter_flush,
    filter_purge, filter_unreadch, filter_tell, filter_seek
};

#ifdef HAVE_ZLIB
static int
flate_close(Xpost_File *f)
{
    Xpost_FlateFile *ff = (Xpost_FlateFile *)f;

    inflateEnd(&ff->strm);
    ff->eod = 1;
    ff->pushback = -1;
    return 0;
}

struct Xpost_File_Methods flate_methods =
{
    flate_readch, filter_writech, flate_close, filter_flush,
    filter_purge, filter_unreadch, filter_tell, filter_seek
};
#endif

/* wrap a malloc'd filter struct in a filetype object */
static Xpost_Object
_filter_object_cons(Xpost_Memory_File *mem, Xpost_File *ff)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;

    if (!ff)
        return invalid;
    f.tag = filetype;
    if (!xpost_memory_table_alloc(mem, sizeof ff, filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
        return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof ff, &ff);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot save file pointer in VM");
        return invalid;
    }
    return f;
}

Xpost_Object xpost_file_cons_filter_hex(Xpost_Memory_File *mem, Xpost_Object src)
{
    Xpost_File *source = xpost_file_get_file_pointer(mem, src);
    Xpost_HexFile *ff;

    if (!source)
        return invalid;
    ff = malloc(sizeof *ff);
    if (ff)
    {
        ff->methods.methods = &hex_methods;
        ff->source = source;
        ff->pushback = -1;
        ff->eod = 0;
    }
    return _filter_object_cons(mem, &ff->methods);
}

Xpost_Object xpost_file_cons_filter_rle(Xpost_Memory_File *mem, Xpost_Object src)
{
    Xpost_File *source = xpost_file_get_file_pointer(mem, src);
    Xpost_RleFile *ff;

    if (!source)
        return invalid;
    ff = malloc(sizeof *ff);
    if (ff)
    {
        ff->methods.methods = &rle_methods;
        ff->source = source;
        ff->pushback = -1;
        ff->eod = 0;
        ff->litrun = 0;
        ff->reprun = 0;
        ff->repbyte = 0;
    }
    return _filter_object_cons(mem, &ff->methods);
}

Xpost_Object xpost_file_cons_filter_subfile(Xpost_Memory_File *mem, Xpost_Object src,
                                            int count, const char *eod, int eodlen)
{
    Xpost_File *source = xpost_file_get_file_pointer(mem, src);
    Xpost_SubFile *ff;

    if (!source || eodlen < 0 || eodlen > 64)
        return invalid;
    ff = malloc(sizeof *ff);
    if (ff)
    {
        ff->methods.methods = &subfile_methods;
        ff->source = source;
        ff->pushback = -1;
        ff->eod = 0;
        ff->count = count;
        memcpy(ff->eodstr, eod, eodlen);
        ff->eodlen = eodlen;
        ff->pendn = ff->pendi = 0;
        /* count 0 with a delimiter behaves as a single occurrence */
        if (eodlen > 0 && ff->count < 1)
            ff->count = 1;
    }
    return _filter_object_cons(mem, &ff->methods);
}

#ifdef HAVE_ZLIB
Xpost_Object xpost_file_cons_filter_flate(Xpost_Memory_File *mem, Xpost_Object src)
{
    Xpost_File *source = xpost_file_get_file_pointer(mem, src);
    Xpost_FlateFile *ff;

    if (!source)
        return invalid;
    ff = malloc(sizeof *ff);
    if (ff)
    {
        ff->methods.methods = &flate_methods;
        ff->source = source;
        ff->pushback = -1;
        ff->eod = 0;
        memset(&ff->strm, 0, sizeof ff->strm);
        if (inflateInit(&ff->strm) != Z_OK)
        {
            free(ff);
            return invalid;
        }
        ff->outn = ff->outi = 0;
    }
    return _filter_object_cons(mem, &ff->methods);
}
#endif

/* construct an ASCII85Decode filter file over a source file object */
Xpost_Object xpost_file_cons_filter_a85(Xpost_Memory_File *mem,
                                        Xpost_Object src)
{
    Xpost_Object f;
    unsigned int ent;
    int ret;
    Xpost_File *source, *ff;

    source = xpost_file_get_file_pointer(mem, src);
    if (!source)
        return invalid;

    f.tag = filetype;
    ff = xpost_filterfile_open_a85(source);
    if (!ff)
        return invalid;
    if (!xpost_memory_table_alloc(mem, sizeof ff, filetype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate file record");
        return invalid;
    }
    f.mark_.padw = ent;
    ret = xpost_memory_put(mem, f.mark_.padw, 0, sizeof ff, &ff);
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
        f.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
        f.tag |= (XPOST_OBJECT_TAG_ACCESS_FILE_WRITE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
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
        fp = xpost_diskfile_fopen(fn, mode, 0, &ret);
        if (fp == NULL)
            return ret;
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
