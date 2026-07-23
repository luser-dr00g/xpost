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

#include <assert.h>
#include <ctype.h>
#include <errno.h> /* errno */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strchr */

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_file.h"
#include "xpost_name.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_array.h"
#include "xpost_op_dict.h"
#include "xpost_op_token.h"

/* large enough for the longest legal string literal (a string's
   length field is 16 bits), which base-85 z-runs can reach from a
   modest number of coded characters */
enum { NBUF = 65540 };

static
int puff(Xpost_Context *ctx,
         char *buf,
         int nbuf,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src),
         void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src));
static
int toke(Xpost_Context *ctx,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src),
         void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src),
         Xpost_Object *retval);

static
int ishash(int c)
{
    return c == '#';
}

static
int isdot(int c)
{
    return c == '.';
}

/* FIXME: return (c == 'e') || (c = 'E') faster ? */
static
int ise(int c)
{
    return strchr("eE", c) != NULL;
}

/* FIXME: see FIXME above */
static
int issign(int c)
{
    return strchr("+-", c) != NULL;
}

/* FIXME: see FIXME above */
static
int isdel(int c)
{
    return strchr("()[]<>{}/%", c) != NULL;
}

static
int isreg(int c)
{
    /* bytes in the binary-token range 128..159 delimit the token they
       follow, exactly as whitespace or a delimiter would (PLRM 3.14.2);
       bytes 160..255 are regular characters and may appear in names */
    return (c != EOF) && !(c >= 128 && c <= 159) &&
           (c >= 128 || (!isspace(c) && !isdel(c)));
}

//int isxdigit (int c) { return strchr("0123456789ABCDEFabcdef", c) != NULL; }

typedef
struct
{
    int (*pred)(int);
    int y;
    int n;
} test;

test fsm_dec[] = {
    /*  int pred(int), y,  n */
    /*       -------- --  -- */
    /* 0 */ { issign,  1,  1 },
    /* 1 */ { isdigit, 2, -1 },
    /* 2 */ { isdigit, 2, -1 } };
static
int accept_dec(int i)
{
    return i == 2;
}

test fsm_rad[] = {
    /* 0 */ { isdigit, 1, -1 },
    /* 1 */ { isdigit, 1,  2 },
    /* 2 */ { ishash,  3, -1 },
    /* 3 */ { isalnum, 4, -1 },
    /* 4 */ { isalnum, 4, -1 } };
static
int accept_rad(int i)
{
    return i == 4;
}

test fsm_real[] = {
    /* 0 */  { issign,  1,   1 },
    /* 1 */  { isdigit, 2,   4 },
    /* 2 */  { isdigit, 2,   3 },
    /* 3 */  { isdot,   6,   7 },
    /* 4 */  { isdot,   5,  -1 },
    /* 5 */  { isdigit, 6,  -1 },
    /* 6 */  { isdigit, 6,   7 },
    /* 7 */  { ise,     8,  -1 },
    /* 8 */  { issign,  9,   9 },
    /* 9 */  { isdigit, 10, -1 },
    /* 10 */ { isdigit, 10, -1 } };
static
int accept_real(int i)
{
    switch (i) { //case 2:
        case 6: case 10: return 1; default: return 0; }
    //return (i & 3) == 2;  // 2, 6 == 2|4, 10 == 2|8
}

static
int fsm_check(char *s,
              int ns,
              test *fsm,
              int (*accept)(int final))
{
    int sta = 0;
    char *sp = s;
    while (sta != -1 && *sp)
    {
        if (sp - s > ns) break;
        if (fsm[sta].pred(*sp))
        {
            sta = fsm[sta].y;
            ++sp;
        }
        else
        {
            sta = fsm[sta].n;
        }
    }
    return accept(sta);
}

static
int grok(Xpost_Context *ctx,
         char *s,
         int ns,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src),
         void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src),
         Xpost_Object *retval)
{
    Xpost_Object obj;
    //printf("grok: %s\n", s);

    if (ns == NBUF)
    {
        XPOST_LOG_ERR("buf maxxed");
        return limitcheck;
    }
    s[ns] = '\0';  //fsm_check & xpost_name_cons  terminate on \0

    { /* plain decimal integers dominate; scan them without the fsms */
        char *p = s;
        if (*p == '+' || *p == '-')
            p++;
        if (isdigit((unsigned char)*p))
        {
            do { p++; } while (isdigit((unsigned char)*p));
            if (p - s == ns)
            {
                long num;
                errno = 0;
                num = strtol(s, NULL, 10);
                if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
                {
                    XPOST_LOG_ERR("integer out of range");
                    return limitcheck;
                }
                *retval = xpost_int_cons(num);
                return 0;
            }
        }
    }

    /* a token that does not start with a digit, sign or dot cannot
       match any of the numeric forms */
    if (!isdigit((unsigned char)*s) && *s != '+' && *s != '-' && *s != '.')
        goto not_a_number;

    if (fsm_check(s, ns, fsm_dec, accept_dec))
    {
        long num;
        num = strtol(s, NULL, 10);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
        {
            XPOST_LOG_ERR("integer out of range");
            return limitcheck;
        }
        //return xpost_int_cons(num);
        *retval = xpost_int_cons(num);
        return 0;
    }

    else if (fsm_check(s, ns, fsm_rad, accept_rad))
    {
        long base, num;
        base = strtol(s, &s, 10);
        if ((base > 36) || (base < 2))
        {
            XPOST_LOG_ERR("bad radix");
            return limitcheck;
        }
        errno = 0;
        num = strtol(s + 1, NULL, base);
        if ((num == LONG_MAX || num == LONG_MIN) && errno == ERANGE)
        {
            XPOST_LOG_ERR("radixnumber out of range");
            return limitcheck;
        }
        //return xpost_int_cons(num);
        *retval = xpost_int_cons(num);
        return 0;
    }

    else if (fsm_check(s, ns, fsm_real, accept_real))
    {
        double num;
        num = strtod(s, NULL);
        if ((num == HUGE_VAL || num == -HUGE_VAL) && errno == ERANGE)
        {
            XPOST_LOG_ERR("real out of range");
            return limitcheck;
        }
        //return xpost_real_cons(num);
        *retval = xpost_real_cons((real)num);
        return 0;
    }

    else
      not_a_number:
        switch(*s)
        {
            case '(':
            {
                int c, defer = 1;
                char *sp = s;
                while (defer && (c = next(ctx, src)) != EOF)
                {
                    switch(c)
                    {
                        case '(': ++defer; break;
                        case ')': --defer; break;
                        case '\r':
                        {
                            /* an end-of-line marker inside a string is
                               one newline character, whichever of the
                               three conventions wrote it */
                            int c2 = next(ctx, src);

                            if (c2 != '\n' && c2 != EOF)
                                back(ctx, c2, src);
                            c = '\n';
                            break;
                        }
                        case '\\':
                            switch(c = next(ctx, src))
                            {
                                case '\n': continue;
                                case '\r':
                                {
                                    /* an escaped end-of-line joins the
                                       lines: nothing is inserted, for
                                       CR alone or CR LF */
                                    int c2 = next(ctx, src);

                                    if (c2 != '\n' && c2 != EOF)
                                        back(ctx, c2, src);
                                    continue;
                                }
                                case 'a': c = '\a'; break;
                                case 'b': c = '\b'; break;
                                case 'f': c = '\f'; break;
                                case 'n': c = '\n'; break;
                                case 'r': c = '\r'; break;
                                case 't': c = '\t'; break;
                                case 'v': c = '\v'; break;
                                default:
                                    if (c >= '0' && c <= '7')
                                    {
                                        /* up to three octal digits, and
                                           only octal ones: 8 and 9 end the
                                           escape, and the character after
                                           the third digit is not part of it */
                                        int t = c - '0', n = 1;
                                        while (n < 3)
                                        {
                                            int d = next(ctx, src);
                                            if (d >= '0' && d <= '7')
                                            {
                                                t = t * 8 + (d - '0');
                                                ++n;
                                            }
                                            else
                                            {
                                                if (d != EOF) back(ctx, d, src);
                                                break;
                                            }
                                        }
                                        c = t & 0xff;
                                    }
                            }
                    }
                    if (!defer) break;
                    if (sp - s > NBUF)
                    {
                        XPOST_LOG_ERR("string exceeds buf");
                        return limitcheck;
                    }
                    else *sp++ = c;
                }
                obj = xpost_string_cons(ctx, sp - s, s);
                if (xpost_object_get_type(obj) == nulltype)
                    return VMerror;
                //return xpost_object_cvlit(obj);
                *retval = xpost_object_cvlit(obj);
                return 0;
            }

            case '<':
            {
                int c;
                char d;
                const char *x = "0123456789ABCDEF";
                char *sp = s;
                c = next(ctx, src);
                if (c == '<')
                {
                    //return xpost_object_cvx(xpost_name_cons(ctx, "<<"));
                    *retval = xpost_object_cvx(xpost_name_cons(ctx, "<<"));
                    return 0;
                }
                if (c == '~')
                {
                    /* <~ ... ~> ASCII base-85 string literal (PLRM
                       3.2.2): five coded characters carry four bytes,
                       'z' abbreviates four zeros, a short final group
                       drops its pad bytes, whitespace falls out */
                    unsigned int grp[5];
                    int n = 0;

                    for (c = next(ctx, src); c != EOF; c = next(ctx, src))
                    {
                        unsigned int tuple;
                        int k;

                        if (isspace(c))
                            continue;
                        if (c == '~')
                        {
                            c = next(ctx, src);
                            if (c != '>')
                            {
                                XPOST_LOG_ERR("malformed base-85 terminator");
                                return syntaxerror;
                            }
                            break;
                        }
                        if (c == 'z' && n == 0)
                        {
                            if (sp - s + 4 > NBUF)
                            {
                                XPOST_LOG_ERR("base-85 string exceeds buf");
                                return limitcheck;
                            }
                            *sp++ = 0; *sp++ = 0; *sp++ = 0; *sp++ = 0;
                            continue;
                        }
                        if (c < '!' || c > 'u')
                        {
                            XPOST_LOG_ERR("character %d in base-85 string", c);
                            return syntaxerror;
                        }
                        grp[n++] = c - '!';
                        if (n < 5)
                            continue;
                        tuple = 0;
                        for (k = 0; k < 5; k++)
                            tuple = tuple * 85 + grp[k];
                        if (sp - s + 4 > NBUF)
                        {
                            XPOST_LOG_ERR("base-85 string exceeds buf");
                            return limitcheck;
                        }
                        *sp++ = (tuple >> 24) & 0xff;
                        *sp++ = (tuple >> 16) & 0xff;
                        *sp++ = (tuple >> 8) & 0xff;
                        *sp++ = tuple & 0xff;
                        n = 0;
                    }
                    if (n == 1)
                    {
                        XPOST_LOG_ERR("dangling base-85 character");
                        return syntaxerror;
                    }
                    if (n > 1)
                    {
                        unsigned int tuple = 0;
                        int k, nbytes = n - 1;

                        for (k = 0; k < 5; k++)
                            tuple = tuple * 85 + (k < n ? grp[k] : 84);
                        if (sp - s + nbytes > NBUF)
                        {
                            XPOST_LOG_ERR("base-85 string exceeds buf");
                            return limitcheck;
                        }
                        for (k = 0; k < nbytes; k++)
                            *sp++ = (tuple >> (24 - 8 * k)) & 0xff;
                    }
                    obj = xpost_string_cons(ctx, sp - s, s);
                    if (xpost_object_get_type(obj) == nulltype)
                        return VMerror;
                    *retval = xpost_object_cvlit(obj);
                    return 0;
                }
                for ( ; c != '>' && c != EOF; c = next(ctx, src))
                {
                    if (isspace(c))
                        continue;
                    if (isxdigit(c))
                        c = strchr(x, toupper(c)) - x;
                    else
                    {
                        XPOST_LOG_ERR("non-hex digit in hex string");
                        return syntaxerror;
                    }
                    d = c << 4; // hi nib
                    while (isspace(c = next(ctx, src)))
                        /**/;
                    if (isxdigit(c))
                        c = strchr(x, toupper(c)) - x;
                    else if (c == '>')
                    {
                        back(ctx, c, src); // pushback for next iter
                        c = 0;             // pretend it got a 0
                    }
                    else
                    {
                        XPOST_LOG_ERR("non-hex digit in hex string");
                        return syntaxerror;
                    }
                    d |= c;
                    if (sp - s > NBUF)
                    {
                        XPOST_LOG_ERR("hexstring exceeds buf");
                        return limitcheck;
                    }
                    *sp++ = d;
                }
                obj = xpost_string_cons(ctx, sp - s, s);
                if (xpost_object_get_type(obj) == nulltype)
                    return VMerror;
                //return xpost_object_cvlit(obj);
                *retval = xpost_object_cvlit(obj);
                return 0;
            }

            case '>':
            {
                int c;
                if ((c = next(ctx, src)) == '>')
                {
                    //return xpost_object_cvx(xpost_name_cons(ctx, ">>"));
                    *retval = xpost_object_cvx(xpost_name_cons(ctx, ">>"));
                    return 0;
                }
                else
                {
                    XPOST_LOG_ERR("bare angle bracket");
                    return syntaxerror;
                }
            }
            return unregistered; //not reached

            case '{':
            { // This is the one part that makes it a recursive-descent parser
                /* each level is a toke() frame carrying a 64 KB token buffer;
                   cap the nesting well within the C stack and raise
                   limitcheck beyond it. The counter unwinds on the single
                   exit below, so scans stay balanced. */
                enum { PROC_NEST_MAX = 100 };
                static int depth = 0;
                int ret;
                Xpost_Object tail;

                if (++depth > PROC_NEST_MAX)
                {
                    --depth;
                    XPOST_LOG_ERR("procedure nesting too deep");
                    return limitcheck;
                }
                tail = xpost_name_cons(ctx, "}");
                xpost_stack_push(ctx->lo, ctx->os, mark);
                ret = 0;
                while (1)
                {
                    Xpost_Object t;
                    ret = toke(ctx, src, next, back, &t);
                    if (ret)
                        break;
                    /* the source ended inside the procedure: the
                       scanner answers a null there and nowhere else,
                       a literal null in the text being a name */
                    if (xpost_object_get_type(t) == nulltype)
                    {
                        XPOST_LOG_ERR("end of input inside a procedure");
                        ret = syntaxerror;
                        break;
                    }
                    if ((xpost_object_get_type(t) == nametype) &&
                        (xpost_dict_compare_objects(ctx, t, tail) == 0))
                        break;
                    xpost_stack_push(ctx->lo, ctx->os, t);
                }
                if (ret == 0)
                {
                    ret = xpost_op_array_to_mark(ctx);  // ie. the /] operator
                    if (ret == 0)
                        *retval = xpost_object_cvx(xpost_stack_pop(ctx->lo, ctx->os));
                }
                --depth;
                return ret;
            }

            case '/':
            {
                *s = next(ctx, src);
                //ns = puff(ctx, s, NBUF, src, next, back);
                if (ns && *s == '/')
                {
                    Xpost_Object ret;
                    ns = puff(ctx, s, NBUF, src, next, back);
                    if (ns == NBUF)
                    {
                        XPOST_LOG_ERR("immediate name exceeds buf");
                        return limitcheck;
                    }
                    s[ns] = '\0';
                    //xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx(xpost_name_cons(ctx, s)));
                    //xpost_operator_exec(ctx, xpost_operator_cons(ctx, "load", NULL,0,0).mark_.padw);
                    if (DEBUGLOAD)
                        printf("\ntoken: loading immediate name %s\n", s);
                    xpost_op_any_load(ctx, xpost_object_cvx(xpost_name_cons(ctx, s)));
                    ret = xpost_stack_pop(ctx->lo, ctx->os);
                    if (DEBUGLOAD)
                        xpost_object_dump(ret);
                    //return ret;
                    *retval = ret;
                    return 0;
                }
                else
                {
                    if ((isspace)(*s))
                    {
                        ns = 0;
                    }
                    else if (isdel(*s))
                    {
                        back(ctx, *s, src);
                        ns = 0;
                    }
                    else
                    {
                        ns += puff(ctx, s + 1, NBUF - 1, src, next, back);
                    }
                }
                if (ns == NBUF)
                {
                    XPOST_LOG_ERR("name exceeds buf");
                    return limitcheck;
                }
                //printf("grok:/%s\n", s);
                s[ns] = '\0';
                //return xpost_object_cvlit(xpost_name_cons(ctx, s));
                *retval = xpost_object_cvlit(xpost_name_cons(ctx, s));
                return 0;
            }
            default:
            {
                //return xpost_object_cvx(xpost_name_cons(ctx, s));
                *retval = xpost_object_cvx(xpost_name_cons(ctx, s));
                return 0;
            }
        }
}

/* read until a non-whitespace, non-comment char.
   "prime" the buffer.  */
static
int snip(Xpost_Context *ctx,
         char *buf,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src))
{
    int c;
    do {
        c = next(ctx, src);
        if (c == '%')
        {
            do {
                c = next(ctx, src);
            } while(c != '\n' && c != '\f' && c != EOF);
        }
    } while(c != EOF && isspace(c));
    if (c == EOF) return 0;
    *buf = c;
    return 1; // true, and size of buffer
}

/* read in a token up to delimiter
   read into buf any regular characters,
   if we read one too many, put it back, unless whitespace. */
static
int puff(Xpost_Context *ctx,
         char *buf,
         int nbuf,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src),
         void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src))
{
    int c;
    char *s = buf;
    while (isreg(c = next(ctx, src)))
    {
        if (s - buf >= nbuf) return 0;
        *s++ = c;
    }
    if (c == '\r')
    {
        /* an end-of-line marker is one white-space character however
           written (PLRM 3.8): a CR delimiter claims its LF */
        int c2 = next(ctx, src);
        if (c2 != '\n' && c2 != EOF)
            back(ctx, c2, src);
    }
    else if (!isspace(c) && c != EOF)
        back(ctx, c, src);
    return s - buf;
}


/* Binary tokens (PLRM 3.14.2): a byte in 128..159 read where a token
   is expected introduces a fixed-format encoded object rather than
   ASCII syntax. Packaged program bodies are written almost entirely
   in these. Only the single-object token forms appear in such
   bodies; binary object sequences (128..131) and the composite forms
   are not accepted. */

/* The standard system name table (PLRM 3rd ed. Appendix F):
   binary name tokens carry an index into this table. Entries
   match the encoding emitted by Level 2 producers; unassigned
   indices are NULL. */
static const char * const _bin_sysname[256] =
{
    "abs", "add", "aload", "anchorsearch",
    "and", "arc", "arcn", "arct",
    "arcto", "array", "ashow", "astore",
    "awidthshow", "begin", "bind", "bitshift",
    "ceiling", "charpath", "clear", "cleartomark",
    "clip", "clippath", "closepath", "concat",
    "concatmatrix", "copy", "count", "counttomark",
    "currentcmykcolor", "currentdash", "currentdict", "currentfile",
    "currentfont", "currentgray", "currentgstate", "currenthsbcolor",
    "currentlinecap", "currentlinejoin", "currentlinewidth", "currentmatrix",
    "currentpoint", "currentrgbcolor", "currentshared", "curveto",
    "cvi", "cvlit", "cvn", "cvr",
    "cvrs", "cvs", "cvx", "def",
    "defineusername", "dict", "div", "dtransform",
    "dup", "end", "eoclip", "eofill",
    "eoviewclip", "eq", "exch", "exec",
    "exit", "file", "fill", "findfont",
    "flattenpath", "floor", "flush", "flushfile",
    "for", "forall", "ge", "get",
    "getinterval", "grestore", "gsave", "gstate",
    "gt", "identmatrix", "idiv", "idtransform",
    "if", "ifelse", "image", "imagemask",
    "index", "ineofill", "infill", "initviewclip",
    "inueofill", "inufill", "invertmatrix", "itransform",
    "known", "le", "length", "lineto",
    "load", "loop", "lt", "makefont",
    "matrix", "maxlength", "mod", "moveto",
    "mul", "ne", "neg", "newpath",
    "not", "null", "or", "pathbbox",
    "pathforall", "pop", "print", "printobject",
    "put", "putinterval", "rcurveto", "read",
    "readhexstring", "readline", "readstring", "rectclip",
    "rectfill", "rectstroke", "rectviewclip", "repeat",
    "restore", "rlineto", "rmoveto", "roll",
    "rotate", "round", "save", "scale",
    "scalefont", "search", "selectfont", "setbbox",
    "setcachedevice", "setcachedevice2", "setcharwidth", "setcmykcolor",
    "setdash", "setfont", "setgray", "setgstate",
    "sethsbcolor", "setlinecap", "setlinejoin", "setlinewidth",
    "setmatrix", "setrgbcolor", "setshared", "shareddict",
    "show", "showpage", "stop", "stopped",
    "store", "string", "stringwidth", "stroke",
    "strokepath", "sub", "systemdict", "token",
    "transform", "translate", "truncate", "type",
    "uappend", "ucache", "ueofill", "ufill",
    "undef", "upath", "userdict", "ustroke",
    "viewclip", "viewclippath", "where", "widthshow",
    "write", "writehexstring", "writeobject", "writestring",
    "wtranslation", "xor", "xshow", "xyshow",
    "yshow", "FontDirectory", "SharedFontDirectory", "Courier",
    "Courier-Bold", "Courier-BoldOblique", "Courier-Oblique", "Helvetica",
    "Helvetica-Bold", "Helvetica-BoldOblique", "Helvetica-Oblique", "Symbol",
    "Times-Bold", "Times-BoldItalic", "Times-Italic", "Times-Roman",
    "execuserobject", "currentcolor", "currentcolorspace", "currentglobal",
    "execform", "filter", "findresource", "globaldict",
    "makepattern", "setcolor", "setcolorspace", "setglobal",
    "setpagedevice", "setpattern", NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

/* read n payload bytes, failing on end of input */
static
int bt_read(Xpost_Context *ctx,
            Xpost_Object *src,
            int (*next)(Xpost_Context *ctx, Xpost_Object *src),
            unsigned char *p,
            int n)
{
    int i, c;

    for (i = 0; i < n; i++)
    {
        c = next(ctx, src);
        if (c == EOF)
            return 0;
        p[i] = (unsigned char)c;
    }
    return 1;
}

/* Number representation byte, shared by the fixed-point token (137)
   and homogeneous number arrays (149): the low bits select width and
   scale, bit 7 selects byte order.
     0..31    32-bit fixed point, scale 0..31
     32..47   16-bit fixed point, scale 0..15
     48       32-bit IEEE real
     49       32-bit native real
   A scale of zero yields an integer; any other scale divides the
   signed fixed value by 2^scale into a real. */
static
int bt_rep_size(unsigned int rep)
{
    unsigned int r = rep & 127;

    if (r <= 31 || r == 48 || r == 49)
        return 4;
    if (r <= 47)
        return 2;
    return 0;
}

static
int bt_rep_number(unsigned int rep, const unsigned char *p, Xpost_Object *retval)
{
    unsigned int r = rep & 127;
    int le = rep >= 128;

    if (r == 48 || r == 49)
    {
        unsigned int v;
        float f;

        if (r == 49)
            memcpy(&v, p, 4);      /* native order */
        else if (le)
            v = ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0];
        else
            v = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
        memcpy(&f, &v, 4);
        *retval = xpost_real_cons((real)f);
        return 0;
    }
    if (r <= 31)
    {
        unsigned int v = le
            ? ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0]
            : ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
        int i = (int)v;

        if (r == 0)
            *retval = xpost_int_cons(i);
        else
            *retval = xpost_real_cons((real)(i / (double)(1u << r)));
        return 0;
    }
    if (r <= 47)
    {
        unsigned short v = le ? (unsigned short)((p[1] << 8) | p[0])
                              : (unsigned short)((p[0] << 8) | p[1]);
        short i = (short)v;
        unsigned int scale = r - 32;

        if (scale == 0)
            *retval = xpost_int_cons(i);
        else
            *retval = xpost_real_cons((real)(i / (double)(1u << scale)));
        return 0;
    }
    return syntaxerror;
}

/* Binary object sequence (PLRM 3.14.1): a self-delimiting block of
   encoded objects. The header carries the top-level count and total
   length; each object is an 8-byte record, with composite bodies
   (array element records, string and name text) at record-relative
   offsets in the remainder. The whole sequence scans to one
   executable array of the top-level objects, which the interpreter
   executes (unlike a brace procedure, which it defers). */
static
int bt_seq_object(Xpost_Context *ctx,
                  const unsigned char *buf,
                  unsigned int buflen,
                  unsigned int base,
                  unsigned int recoff,
                  int le,
                  int depth,
                  Xpost_Object *retval)
{
    const unsigned char *p;
    unsigned int type, xflag, length, value;
    Xpost_Object obj;

    if (depth > 32 || recoff + 8 > buflen)
        return syntaxerror;
    p = buf + recoff;
    xflag = p[0] & 0x80;
    type = p[0] & 0x7f;
    length = le ? (unsigned int)((p[3] << 8) | p[2])
                : (unsigned int)((p[2] << 8) | p[3]);
    value = le
        ? ((unsigned int)p[7] << 24) | ((unsigned int)p[6] << 16) | ((unsigned int)p[5] << 8) | p[4]
        : ((unsigned int)p[4] << 24) | ((unsigned int)p[5] << 16) | ((unsigned int)p[6] << 8) | p[7];

    /* records that use the value as an offset (string and name text,
       array elements) are only defined for the low-order header
       forms; offsets are relative to the start of the object records */
    switch (type)
    {
        case 0:  /* null: no other content is valid */
            if (length != 0 || value != 0)
                return syntaxerror;
            obj = null;
            break;
        case 1:  /* integer */
            obj = xpost_int_cons((integer)(int)value);
            break;
        case 2:  /* real: the native-real formats travel in the
                    sequence's byte order all the same, as the
                    deployed writers have it */
            {
                float f;

                memcpy(&f, &value, 4);
                obj = xpost_real_cons((real)f);
            }
            break;
        case 3:  /* name: text at offset, or a user name table index */
        case 6:  /* as 3, but the value replaces the name at scan time */
            if (length == 0)
            {
                /* no operator populates the user name table */
                XPOST_LOG_ERR("user name index %d: no user name table", (int)value);
                return undefined;
            }
            else
            {
                char *nm;
                if (!le || length > 127
                 || base + value + length > buflen)
                    return syntaxerror;
                nm = malloc(length + 1);
                if (!nm)
                    return VMerror;
                memcpy(nm, buf + base + value, length);
                nm[length] = '\0';
                obj = xpost_name_cons(ctx, nm);
                free(nm);
            }
            if (xpost_object_get_type(obj) == invalidtype)
                return VMerror;
            if (type == 6)
            {
                int ret = xpost_op_any_load(ctx, xpost_object_cvx(obj));
                if (ret)
                    return ret;
                obj = xpost_stack_pop(ctx->lo, ctx->os);
                *retval = obj;
                return 0;
            }
            break;
        case 4:  /* boolean */
            if (value > 1)
                return syntaxerror;
            obj = xpost_bool_cons(value != 0);
            break;
        case 5:  /* string */
            if (length && (!le || base + value + length > buflen))
                return syntaxerror;
            obj = xpost_string_cons(ctx, length, (char *)(buf + base + value));
            if (xpost_object_get_type(obj) == nulltype)
                return VMerror;
            break;
        case 9:  /* array: length records at the offset */
            {
                unsigned int i;
                int ret;

                if (length && (!le || base + value + (unsigned int)length * 8 > buflen))
                    return syntaxerror;
                obj = xpost_array_cons(ctx, length);
                if (xpost_object_get_type(obj) == nulltype)
                    return VMerror;
                for (i = 0; i < length; i++)
                {
                    Xpost_Object el;

                    ret = bt_seq_object(ctx, buf, buflen, base, base + value + i * 8, le, depth + 1, &el);
                    if (ret)
                        return ret;
                    xpost_array_put(ctx, obj, i, el);
                }
            }
            break;
        case 10: /* mark */
            if (length != 0 || value != 0)
                return syntaxerror;
            obj = mark;
            break;
        default:
            XPOST_LOG_ERR("unsupported type %u in binary object sequence", type);
            return syntaxerror;
    }
    /* the executable attribute is meaningful for names, strings and
       arrays; scalar records carry it without effect */
    if (type == 3 || type == 5 || type == 9)
        *retval = xflag ? xpost_object_cvx(obj) : xpost_object_cvlit(obj);
    else
        *retval = xpost_object_cvlit(obj);
    return 0;
}

static
int bt_sequence(Xpost_Context *ctx,
                unsigned int t,
                Xpost_Object *src,
                int (*next)(Xpost_Context *ctx, Xpost_Object *src),
                Xpost_Object *retval)
{
    int le = (t == 129) || (t == 131);
    unsigned char hdr[8];
    unsigned int count, length, hdrlen;
    unsigned char *buf;
    Xpost_Object arr;
    unsigned int i;
    int ret;

    if (!bt_read(ctx, src, next, hdr + 1, 3))
        return syntaxerror;
    if (hdr[1] != 0)
    {
        count = hdr[1];
        length = le ? (unsigned int)((hdr[3] << 8) | hdr[2])
                    : (unsigned int)((hdr[2] << 8) | hdr[3]);
        hdrlen = 4;
    }
    else
    {
        /* extended header: 2-byte count and 4-byte length follow */
        if (!bt_read(ctx, src, next, hdr + 4, 4))
            return syntaxerror;
        count = le ? (unsigned int)((hdr[3] << 8) | hdr[2])
                   : (unsigned int)((hdr[2] << 8) | hdr[3]);
        length = le
            ? ((unsigned int)hdr[7] << 24) | ((unsigned int)hdr[6] << 16) | ((unsigned int)hdr[5] << 8) | hdr[4]
            : ((unsigned int)hdr[4] << 24) | ((unsigned int)hdr[5] << 16) | ((unsigned int)hdr[6] << 8) | hdr[7];
        hdrlen = 8;
    }
    if (length < hdrlen + count * 8u || length > 1u << 20)
        return syntaxerror;

    /* buffer the whole sequence; record offsets address into it */
    buf = malloc(length);
    if (!buf)
        return VMerror;
    memset(buf, 0, hdrlen);
    if (!bt_read(ctx, src, next, buf + hdrlen, (int)(length - hdrlen)))
    {
        free(buf);
        return syntaxerror;
    }

    arr = xpost_array_cons(ctx, count);
    if (xpost_object_get_type(arr) == nulltype)
    {
        free(buf);
        return VMerror;
    }
    for (i = 0; i < count; i++)
    {
        Xpost_Object el;

        ret = bt_seq_object(ctx, buf, length, hdrlen, hdrlen + i * 8, le, 0, &el);
        if (ret)
        {
            free(buf);
            return ret;
        }
        xpost_array_put(ctx, arr, i, el);
    }
    free(buf);
    /* the sequence executes; only brace procedures defer */
    ctx->scanner_defer = 0;
    *retval = xpost_object_cvx(arr);
    return 0;
}

static
int binary_token(Xpost_Context *ctx,
                 unsigned int t,
                 Xpost_Object *src,
                 int (*next)(Xpost_Context *ctx, Xpost_Object *src),
                 Xpost_Object *retval)
{
    unsigned char p[4];

    switch (t)
    {
        case 128: case 129: case 130: case 131:  /* binary object sequence */
            return bt_sequence(ctx, t, src, next, retval);
        case 137:  /* fixed-point number: representation byte, then value */
            {
                unsigned char q[4];
                int sz;

                if (!bt_read(ctx, src, next, p, 1))
                    return syntaxerror;
                sz = bt_rep_size(p[0]);
                if (sz == 0)
                    return syntaxerror;
                if (!bt_read(ctx, src, next, q, sz))
                    return syntaxerror;
                return bt_rep_number(p[0], q, retval);
            }
        case 147: case 148:  /* user name table: nothing populates it */
            if (!bt_read(ctx, src, next, p, 1))
                return syntaxerror;
            XPOST_LOG_ERR("user name index %d: no user name table", p[0]);
            return undefined;
        case 149:  /* homogeneous number array */
            {
                unsigned int rep, count, i;
                int sz;
                unsigned char *buf;
                Xpost_Object arr;

                if (!bt_read(ctx, src, next, p, 3))
                    return syntaxerror;
                rep = p[0];
                count = rep >= 128 ? (unsigned int)((p[2] << 8) | p[1])
                                   : (unsigned int)((p[1] << 8) | p[2]);
                sz = bt_rep_size(rep);
                if (sz == 0)
                    return syntaxerror;
                buf = malloc((size_t)count * sz + 1);
                if (!buf)
                    return VMerror;
                if (!bt_read(ctx, src, next, buf, (int)(count * sz)))
                {
                    free(buf);
                    return syntaxerror;
                }
                arr = xpost_array_cons(ctx, count);
                if (xpost_object_get_type(arr) == nulltype)
                {
                    free(buf);
                    return VMerror;
                }
                for (i = 0; i < count; i++)
                {
                    Xpost_Object el;
                    int ret = bt_rep_number(rep, buf + (size_t)i * sz, &el);
                    if (ret)
                    {
                        free(buf);
                        return ret;
                    }
                    xpost_array_put(ctx, arr, i, el);
                }
                free(buf);
                *retval = xpost_object_cvlit(arr);
            }
            return 0;
        case 132: case 133:  /* 32-bit integer, high/low order */
            if (!bt_read(ctx, src, next, p, 4))
                return syntaxerror;
            {
                unsigned int v = t == 132
                    ? ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3]
                    : ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0];
                *retval = xpost_int_cons((integer)(int)v);
            }
            return 0;
        case 134: case 135:  /* 16-bit integer, high/low order */
            if (!bt_read(ctx, src, next, p, 2))
                return syntaxerror;
            {
                unsigned short v = t == 134
                    ? (unsigned short)((p[0] << 8) | p[1])
                    : (unsigned short)((p[1] << 8) | p[0]);
                *retval = xpost_int_cons((short)v);
            }
            return 0;
        case 136:  /* 8-bit integer */
            if (!bt_read(ctx, src, next, p, 1))
                return syntaxerror;
            *retval = xpost_int_cons((signed char)p[0]);
            return 0;
        case 138: case 139: case 140:  /* 32-bit real: IEEE high/low, native */
            if (!bt_read(ctx, src, next, p, 4))
                return syntaxerror;
            {
                unsigned int v;
                float r;
                if (t == 139)
                    v = ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0];
                else if (t == 138)
                    v = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
                else
                    memcpy(&v, p, 4);
                memcpy(&r, &v, 4);
                *retval = xpost_real_cons((real)r);
            }
            return 0;
        case 141:  /* boolean */
            if (!bt_read(ctx, src, next, p, 1))
                return syntaxerror;
            if (p[0] > 1)
                return syntaxerror;
            *retval = xpost_bool_cons(p[0] != 0);
            return 0;
        case 142: case 143: case 144:  /* string: 1-byte, 2-byte high/low length */
            {
                unsigned int len;
                unsigned char *buf;
                Xpost_Object obj;

                if (t == 142)
                {
                    if (!bt_read(ctx, src, next, p, 1))
                        return syntaxerror;
                    len = p[0];
                }
                else
                {
                    if (!bt_read(ctx, src, next, p, 2))
                        return syntaxerror;
                    len = t == 143 ? (unsigned int)((p[0] << 8) | p[1])
                                   : (unsigned int)((p[1] << 8) | p[0]);
                }
                buf = malloc(len ? len : 1);
                if (!buf)
                    return VMerror;
                if (!bt_read(ctx, src, next, buf, (int)len))
                {
                    free(buf);
                    return syntaxerror;
                }
                obj = xpost_string_cons(ctx, len, (char *)buf);
                free(buf);
                if (xpost_object_get_type(obj) == nulltype)
                    return VMerror;
                *retval = xpost_object_cvlit(obj);
            }
            return 0;
        case 145: case 146:  /* literal/executable system name */
            if (!bt_read(ctx, src, next, p, 1))
                return syntaxerror;
            if (_bin_sysname[p[0]] == NULL)
            {
                XPOST_LOG_ERR("system name index %d unassigned", p[0]);
                return undefined;
            }
            {
                Xpost_Object n = xpost_name_cons(ctx, (char *)_bin_sysname[p[0]]);
                if (xpost_object_get_type(n) == invalidtype)
                    return VMerror;
                *retval = t == 145 ? xpost_object_cvlit(n) : xpost_object_cvx(n);
            }
            return 0;
        default:
            XPOST_LOG_ERR("unsupported binary token type %u", t);
            return syntaxerror;
    }
}

static
int toke(Xpost_Context *ctx,
         Xpost_Object *src,
         int (*next)(Xpost_Context *ctx, Xpost_Object *src),
         void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src),
         Xpost_Object *retval)
{
    char buf[NBUF]; /* grok() NUL-terminates at the token length */
    int sta;  // status, and size
    Xpost_Object o;
    int ret;

    if (!src)
    {
        XPOST_LOG_ERR("src is NULL");
        return unregistered;
    }

    ctx->scanner_defer = 1;
    sta = snip(ctx, buf, src, next);
    if (!sta)
    {
        *retval = null;
        return 0;
    }
    if ((unsigned char)buf[0] >= 128 && (unsigned char)buf[0] <= 159)
        return binary_token(ctx, (unsigned char)buf[0], src, next, retval);
    if (!isdel(*buf))
        sta += puff(ctx, buf + 1, NBUF - 1, src, next, back);
    ret = grok(ctx, buf, sta, src, next, back, &o);
    if (ret)
        return ret;
    *retval = o;
    return 0;
}


/* file  token  token true
   false
   read token from file */
static
int Fnext(Xpost_Context *ctx,
          Xpost_Object *F)
{
    return xpost_file_getc(xpost_file_get_file_pointer(ctx->lo, *F));
}
static
void Fback(Xpost_Context *ctx,
           int c,
           Xpost_Object *F)
{
    (void)xpost_file_ungetc(xpost_file_get_file_pointer(ctx->lo, *F), c);
}
static
int Ftoken(Xpost_Context *ctx,
           Xpost_Object F)
{
    Xpost_Object t;
    int ret;

    xpost_stack_push(ctx->lo, ctx->hold, F);

    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    ret = toke(ctx, &F, Fnext, Fback, &t);
    if (ret)
        return ret;
    if (xpost_object_get_type(t) != nulltype)
    {
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    }
    else
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    }
    return 0;
}

/* string  token  substring token true
   false
   read token from string */
static
int Snext(Xpost_Context *ctx,
          Xpost_Object *S)
{
    int ret;
    if (S->comp_.sz == 0) return EOF;
    /* the byte must not sign-extend into EOF */
    ret = (unsigned char)xpost_string_get_pointer(ctx, *S)[0];
    ++S->comp_.off;
    --S->comp_.sz;
    return ret;
}
static
void Sback(Xpost_Context *ctx,
           int c,
           Xpost_Object *S)
{
    --S->comp_.off;
    ++S->comp_.sz;
    xpost_string_get_pointer(ctx, *S)[0] = c;
}
static
int Stoken(Xpost_Context *ctx,
           Xpost_Object S)
{
    Xpost_Object t;
    int ret;

    xpost_stack_push(ctx->lo, ctx->hold, S);

    ret = toke(ctx, &S, Snext, Sback, &t);
    if (ret)
        return ret;
    if (xpost_object_get_type(t) != nulltype)
    {
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    }
    else
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    }
    return 0;
}

int xpost_oper_init_token_ops(Xpost_Context *ctx,
                              Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "token", (Xpost_Op_Func)Ftoken, 2, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "token", (Xpost_Op_Func)Stoken, 3, 1, stringtype);
    INSTALL;
    ctx->opcode_shortcuts.token = op.mark_.padw;
    return 0;
}
