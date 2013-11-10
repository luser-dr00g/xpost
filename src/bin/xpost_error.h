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

#ifndef XPOST_ERR_H
#define XPOST_ERR_H

/*
   For a commentary on these macros, see my answer to this SO question
http://stackoverflow.com/questions/6635851/real-world-use-of-x-macros/6636596#6636596

    error() is the internal error call.
    If the jump-point is set, it does a longjmp back to mainloop, which then
    calls onerror()

    Having unwound the "C" stack already,
    onerror() goes on to schedule a (PS) call to err.ps/signalerror and the rest
    of the process proceeds at the postscript level.
   */

#define AS_BARE(a) a ,
/* #define AS_STR(a) #a , /\* defined in ob.h *\/ */

#define ERRORS(_) \
    _(noerror) \
    _(dictfull) \
    _(dictstackoverflow) \
    _(dictstackunderflow) \
    _(execstackoverflow) \
    _(execstackunderflow) /*5*/\
    _(handleerror) \
    _(interrupt) \
    _(invalidaccess) \
    _(invalidexit) \
    _(invalidfileaccess) /*10*/\
    _(invalidfont) \
    _(invalidrestore) \
    _(ioerror) \
    _(limitcheck) \
    _(nocurrentpoint) /*15*/\
    _(rangecheck) \
    _(stackoverflow) \
    _(stackunderflow) \
    _(syntaxerror) \
    _(timeout) /*20*/\
    _(typecheck) \
    _(undefined) \
    _(undefinedfilename) \
    _(undefinedresult) \
    _(unmatchedmark) /*25*/\
    _(unregistered) \
    _(VMerror)
enum err { ERRORS(AS_BARE) };
extern char *errorname[] /*= { ERRORS(AS_STR) }*/;
/* puts(errorname[(enum err)limitcheck]); */

extern volatile char *errormsg;

void error(unsigned err, char *msg);

#endif
