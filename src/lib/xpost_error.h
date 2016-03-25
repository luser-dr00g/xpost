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
 * @file xpost_error.h
 * @brief This file provides the Xpost error functions.
 *
 * This header provides the Xpost error functions.
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#ifndef XPOST_ERROR_H
#define XPOST_ERROR_H

/*
   X-Macro utilities
   For a commentary on these macros, see my answer to this SO question
http://stackoverflow.com/questions/6635851/real-world-use-of-x-macros/6636596#6636596
*/

#define AS_BARE(a) a ,
/* #define AS_STR(a) #a , /\* defined in ob.h *\/ */

/**
 * @brief Macro to generate error identifiers.
 * These error codes are (mostly) defined in the PLRM and can be returned by operator
 * functions and handled at the postscript level. If an operator (including a device
 * function) returns a value outside of this range, the error-name returned to postscript
 * will be /unknownerror.
 * In some circumstances, /unregistered is used also used for this purpose,
 * since it has no documented use in the PLRM.
 * This gives you /unknownerror as a (mostly) freely-available error code for any
 * device-specific testing or such. So `return -1;` in a device function will send this
 * code back to the ps error-handler.
 *
 * An operator function may fail to execute if the operator_exec function cannot match
 * the type signature against the operand stack. It will then return a /typecheck or
 * /stackunderflow error to postscript.
 *
 * contextswitch and ioblock represent requests to the interpreter to change the state
 * of the execution-context. They cannot be caught by postscript error code.
 *
 * yieldtocaller is used to implement the Showpage-Return semantic where xpost
 * returns from xpost_run() after rendering the buffer.
 */
#define ERRORS(_) \
    _(noerror)            /*0*/\
    _(unregistered)            \
    _(dictfull)                \
    _(dictstackoverflow)       \
    _(dictstackunderflow)      \
    _(execstackoverflow)  /*5*/\
    _(execstackunderflow)      \
    _(handleerror)             \
    _(interrupt)               \
    _(invalidaccess)           \
    _(invalidexit)       /*10*/\
    _(invalidfileaccess)       \
    _(invalidfont)             \
    _(invalidrestore)          \
    _(ioerror)                 \
    _(limitcheck)        /*15*/\
    _(nocurrentpoint)          \
    _(rangecheck)              \
    _(stackoverflow)           \
    _(stackunderflow)          \
    _(syntaxerror)       /*20*/\
    _(timeout)                 \
    _(typecheck)               \
    _(undefined)               \
    _(undefinedfilename)       \
    _(undefinedresult)   /*25*/\
    _(unmatchedmark)           \
    _(VMerror)                 \
    _(contextswitch)           \
    _(ioblock)                 \
    _(yieldtocaller)    /* 30*/\
    _(unknownerror)     /* 31 nb. unknownerror is the catch-all and must be last */ \
/* #enddef ERRORS */

/**
 * @brief Error codes for operator return.
 */
enum err { ERRORS(AS_BARE) };

/**
 * @brief Printable string representations of Error codes.
 */
extern char *errorname[] /*= { ERRORS(AS_STR) }*/;
/* puts(errorname[(enum err)limitcheck]); */

/**
 * @}
 */

#endif
