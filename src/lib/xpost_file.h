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

/**
 * @file xpost_file.h
 * @brief This file provides the Xpost functions.
 *
 * This header provides the Xpost management functions.
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#ifndef XPOST_F_H
#define XPOST_F_H

/*
   a filetype object uses .mark_.padw to store the ent
   for the Xpost_File *

   The Xpost_File code for abstract use of files and file-like
   interfaces is a slight variation of an approach described
   by Tim Rentsch.
   */

typedef struct Xpost_File Xpost_File;

typedef struct Xpost_File_Methods
{
    int (*readch)(Xpost_File*);
    int (*writech)(Xpost_File*, int);
    int (*close)(Xpost_File*);
    int (*flush)(Xpost_File*);
    void (*purge)(Xpost_File*);
    int (*unreadch)(Xpost_File*, int);
    long (*tell)(Xpost_File*);
    int (*seek)(Xpost_File*, long);
} Xpost_File_Methods;

struct Xpost_File
{
    Xpost_File_Methods *methods;
};

typedef struct Xpost_DiskFile
{
    Xpost_File methods;
    FILE *file;
} Xpost_DiskFile;

/* interface fgetc
   in preparation for more elaborate cross-platform non-blocking mechanisms
cf. http://stackoverflow.com/questions/20428616/how-to-handle-window-events-while-waiting-for-terminal-input
and http://stackoverflow.com/questions/25506324/how-to-do-pollstdin-or-selectstdin-when-stdin-is-a-windows-console
   */
/**
 * @brief Read a byte from an Xpost_File abstraction.
 */
static inline
int xpost_file_getc(Xpost_File *in)
{
    return in->methods->readch(in);
}

static inline
int xpost_file_putc(Xpost_File *out, int c)
{
    return out->methods->writech(out, c);
}

static inline
int xpost_file_close(Xpost_File *f)
{
    return f->methods->close(f);
}

static inline
int xpost_file_flush(Xpost_File *f)
{
    return f->methods->flush(f);
}

static inline
void xpost_file_purge(Xpost_File *f)
{
    f->methods->purge(f);
}

static inline
int xpost_file_ungetc(Xpost_File *in, int c)
{
    return in->methods->unreadch(in, c);
}

static inline
long xpost_file_tell(Xpost_File *f)
{
    return f->methods->tell(f);
}

static inline
int xpost_file_seek(Xpost_File *f, long offset)
{
    return f->methods->seek(f, offset);
}


/**
 * @brief Construct a file object given a FILE*.
 */
Xpost_Object xpost_file_cons(Xpost_Memory_File *mem, /*@NULL@*/ const FILE *fp);


/**
 * @brief Open and construct a file object given filename and mode.
 */
int xpost_file_open(Xpost_Memory_File *mem, char *fn, char *mode, Xpost_Object *retval);

/**
 * @brief Return the FILE* from the file object.
 */
Xpost_File *xpost_file_get_file_pointer(Xpost_Memory_File *mem, Xpost_Object f);

/**
 * @brief Get the status of the file object.
 */
int xpost_file_get_status(Xpost_Memory_File *mem, Xpost_Object f);

/**
 * @brief Return number of bytes available to read.
 */
int xpost_file_get_bytes_available(Xpost_Memory_File *mem, Xpost_Object f, int *retval);

/**
 * @brief Close the file and deallocate the descriptor in VM.
 */
int xpost_file_object_close(Xpost_Memory_File *mem, Xpost_Object f);

int xpost_file_read(unsigned char *buf, int size, int count, Xpost_File *fp);
int xpost_file_write(const unsigned char *buf, int size, int count, Xpost_File *fp);

/**
 * @brief Read a byte from file object.
 */
Xpost_Object xpost_file_read_byte(Xpost_Memory_File *mem, Xpost_Object f);

/**
 * @brief Write a byte to a file object.
 */
int xpost_file_write_byte(Xpost_Memory_File *mem, Xpost_Object f, Xpost_Object b);

/**
 * @}
 */

#endif
