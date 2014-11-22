/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
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

#ifndef XPOST_ITP_H
#define XPOST_ITP_H

/**
 * @brief the interpreter

 * The interpreter module manages the itpdata structure, allocating
 * contexts from a table, and allocating memory files to the contexts
 * also from tables. The itpdata structure thus encapsulates the entire
 * dynamic state of the interpreter as a whole.
 *
 * The interpreter module also contains functions for eval actions,
 * the core interpreter loop,
   */

/*# define MAXCONTEXT 10 // moved to xpost_context.h. <- must include first!
 */
#define MAXMFILE 10

typedef struct {
    Xpost_Context ctab[MAXCONTEXT];
    unsigned int cid;
    Xpost_Memory_File gtab[MAXMFILE];
    Xpost_Memory_File ltab[MAXMFILE];
    int in_onerror;
} Xpost_Interpreter;


extern Xpost_Interpreter *itpdata;

/* garbage collection does not run during initializing */
int xpost_interpreter_get_initializing(void);
void xpost_interpreter_set_initializing(int i);

Xpost_Context *xpost_interpreter_cid_get_context(unsigned int cid);

/**
 * The event-handler handler.

 * In a multi-threaded configuration, this may not execute at in every eval()
 * but by a superior strategy.
 */
int idleproc (Xpost_Context *ctx);

extern int TRACE;

int xpost_interpreter_init(Xpost_Interpreter *itp, const char *device);
void xpost_interpreter_exit(Xpost_Interpreter *itp);

/*
   Specify the behavior the interpreter should take when executing `showpage`.
   DEFAULT is to print "----showpage----\n" to stdout and read and discard a
   line of text from stdin (ie. wait for return). NOPAUSE bypasses this action
   but still performs a "flush" of the graphics device. RETURN causes the
   interpreter to return control to its caller; the suspended context may be
   resumed by calling xpost_run with an input type of RESUME.
 */
enum Xpost_Showpage_Semantics {
    XPOST_SHOWPAGE_DEFAULT,
    XPOST_SHOWPAGE_NOPAUSE,
    XPOST_SHOWPAGE_RETURN
};

/*
   Specify the interpretation of the outputptr parameter to xpost_create().
   DEFAULT ignores outputptr. FILENAME treats outputptr as a char* to a
   zero-terminated OS path string (@@). BUFFERIN will treat outputptr as an
   unsigned char * and render directly into this memory (*). BUFFEROUT treats
   outputptr as an unsigned char ** and malloc()s a new buffer and assigns
   it to the unsigned char * which outputptr points to.

   (*) not currently implemented.
   (@@) implemented in pgm and ppm devices.
   (###)
 */
enum Xpost_Output_Type {
    XPOST_OUTPUT_DEFAULT,
    XPOST_OUTPUT_FILENAME,
    XPOST_OUTPUT_BUFFERIN,
    XPOST_OUTPUT_BUFFEROUT
};

/*
   Specify the interpretation of the inputptr parameter to xpost_run().
   STRING treats inputptr as a char * to an zero-terminated ascii string,
   writes the whole string into a temporary file and falls through to the
   FILEPTR case. FILEPTR treats inputptr as a FILE *, creates a postscript
   file object and pushes it on the execution stack (scheduling it to 
   execute). FILENAME treats inputptr as a char * to a zero-terminated OS
   path string, and pushes the path string itself, scheduling a procedure
   to execute it. RESUME bypasses any execution scheduling.
 */
enum Xpost_Input_Type {
    XPOST_INPUT_STRING,
    XPOST_INPUT_FILENAME,
    XPOST_INPUT_FILEPTR,
    XPOST_INPUT_RESUME
};

enum Xpost_Set_Size {
    XPOST_IGNORE_SIZE,
    XPOST_USE_SIZE
};

/* 3 simple top-level functions */

/*
   The is_installed parameter controls whether the interpreter should look
   to the standard locations for its postscript initialization files or
   it should look for these files in "$CWD/data/". 
 */
Xpost_Context *xpost_create(const char *device,
                 enum Xpost_Output_Type output_type,
                 const void *outputptr,
                 enum Xpost_Showpage_Semantics semantics,
                 int quiet,
                 int is_installed,
                 enum Xpost_Set_Size set_size,
                 int width,
                 int height);

int xpost_run(Xpost_Context *ctx, enum Xpost_Input_Type input_type,
               const void *inputptr);

void xpost_destroy(Xpost_Context *ctx);

#endif
