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

#include <assert.h>
#include <ctype.h>
#include <errno.h> /* errno */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strchr */

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_file.h"
#include "xpost_operator.h"
#include "xpost_name.h"
#include "xpost_op_array.h"
#include "xpost_op_dict.h"
#include "xpost_op_token.h"

enum { NBUF = BUFSIZ };

static
int puff (context *ctx,
          char *buf,
          int nbuf,
          Xpost_Object *src,
          int (*next)(context *ctx, Xpost_Object *src),
          void (*back)(context *ctx, int c, Xpost_Object *src));
static
Xpost_Object toke (context *ctx,
             Xpost_Object *src,
             int (*next)(context *ctx, Xpost_Object *src),
             void (*back)(context *ctx, int c, Xpost_Object *src));

static
int ishash (int c)
{
    return c == '#';
}

static
int isdot (int c)
{
    return c == '.';
}

static
int ise (int c)
{
    return strchr("eE", c) != NULL;
}

static
int issign (int c)
{
    return strchr("+-", c) != NULL;
}

static
int isdel (int c)
{
    return strchr("()[]<>{}/%", c) != NULL;
}

static
int isreg (int c)
{
    return (c!=EOF) && (!isspace(c)) && (!isdel(c));
}

//int isxdigit (int c) { return strchr("0123456789ABCDEFabcdef", c) != NULL; }

typedef
struct {
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
int fsm_check (char *s,
               int ns,
               test *fsm,
               int (*accept)(int final))
{
    int sta = 0;
    char *sp = s;
    while (sta != -1 && *sp) {
        if (sp-s > ns) break;
        if (fsm[sta].pred(*sp)) {
            sta = fsm[sta].y;
            ++sp;
        } else {
            sta = fsm[sta].n;
        }
    }
    return accept(sta);
}

static
Xpost_Object grok (context *ctx,
             char *s,
             int ns,
             Xpost_Object *src,
             int (*next)(context *ctx, Xpost_Object *src),
             void (*back)(context *ctx, int c, Xpost_Object *src))
{
    if (ns == NBUF) error(limitcheck, "grok buf maxxed");
    s[ns] = '\0';  //fsm_check & consname  terminate on \0

    if (fsm_check(s, ns, fsm_dec, accept_dec)) {
        long num;
        num = strtol(s, NULL, 10);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
            error(limitcheck, "grok integer out of range");
        return xpost_cons_int(num);
    }

    else if (fsm_check(s, ns, fsm_rad, accept_rad)) {
        long base, num;
        base = strtol(s, &s, 10);
        if (base > 36 || base < 2)
            error(limitcheck, "grok bad radix");
        num = strtol(s+1, NULL, base);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
            error(limitcheck, "grok radixnumber out of range");
        return xpost_cons_int(num);
    }

    else if (fsm_check(s, ns, fsm_real, accept_real)) {
        double num;
        num = strtod(s, NULL);
        if ((num == HUGE_VAL || num == -HUGE_VAL) && errno==ERANGE)
            error(limitcheck, "grok real out of range");
        return xpost_cons_real(num);
    }

    else switch(*s) {

    case '(': {
                  int c, defer = 1;
                  char *sp = s;
                  while (defer && (c = next(ctx, src)) != EOF) {
                      switch(c) {
                      case '(': ++defer; break;
                      case ')': --defer; break;
                      case '\\':
                          switch(c = next(ctx, src)) {
                          case '\n': continue;
                          case 'a': c = '\a'; break;
                          case 'b': c = '\b'; break;
                          case 'f': c = '\f'; break;
                          case 'n': c = '\n'; break;
                          case 'r': c = '\r'; break;
                          case 't': c = '\t'; break;
                          case 'v': c = '\v'; break;
                          default:
                              if (isdigit(c)) {
                                  int t = 0, n = 0;
                                  do {
                                      t *= 8;
                                      t += c - '0';
                                      ++n;
                                      c = next(ctx, src);
                                  } while (isdigit(c) && n < 3);
                                  if (!isdigit(c)) back(ctx, c, src);
                                  c = t;
                              }
                          }
                      }
                      if (!defer) break;
                      if (sp-s > NBUF) error(limitcheck, "grok string exceeds buf");
                      else *sp++ = c;
                  }
                  return xpost_object_cvlit(consbst(ctx, sp-s, s));
              }

    case '<': {
                  int c;
                  char d, *x = "0123456789ABCDEF", *sp = s;
                  c = next(ctx, src);
                  if (c == '<') {
                      return xpost_object_cvx(consname(ctx, "<<"));
                  }
                  back(ctx, c, src);
                  while (c = next(ctx, src), c != '>' && c != EOF) {
                      if (isspace(c)) continue;
                      if (isxdigit(c)) c = strchr(x, toupper(c)) - x;
                      else error(syntaxerror, "grok");
                      d = c << 4; // hi nib
                      while(isspace(c = next(ctx, src)))
                          /**/;
                      if (isxdigit(c))
                          c = strchr(x, toupper(c)) - x;
                      else if (c == '>') {
                          back(ctx, c, src); // pushback for next iter
                          c = 0;             // pretend it got a 0
                      } else error(syntaxerror, "grok");
                      d |= c;
                      if (sp-s > NBUF) error(limitcheck, "grok hexstring exceeds buf");
                      *sp++ = d;
                  }
                  return xpost_object_cvlit(consbst(ctx, sp-s, s));
              }

    case '>': {
                  int c;
                  if ((c = next(ctx, src)) == '>') {
                      return xpost_object_cvx(consname(ctx, ">>"));
                  } else error(syntaxerror, "grok bare angle bracket >");
              }
              return null; //not reached

    case '{': { // This is the one part that makes it a recursive-descent parser
                  Xpost_Object tail;
                  tail = consname(ctx, "}");
                  xpost_stack_push(ctx->lo, ctx->os, mark);
                  while (1) {
                      Xpost_Object t = toke(ctx, src, next, back);
                      if (objcmp(ctx, t, tail) == 0)
                          break;
                      xpost_stack_push(ctx->lo, ctx->os, t);
                  }
                  arrtomark(ctx);  // ie. the /] operator
                  return xpost_object_cvx(xpost_stack_pop(ctx->lo, ctx->os));
              }

    case '/': {
                  *s = next(ctx, src);
                  //ns = puff(ctx, s, NBUF, src, next, back);
                  if (ns && *s == '/') {
                      Xpost_Object ret;
                      ns = puff(ctx, s, NBUF, src, next, back);
                      if (ns == NBUF) error(limitcheck, "grok immediate name exceeds buf");
                      s[ns] = '\0';
                      //xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx(consname(ctx, s)));
                      //opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
                      if (DEBUGLOAD)
                          printf("\ntoken: loading immediate name %s\n", s);
                      Aload(ctx, xpost_object_cvx(consname(ctx, s)));
                      ret = xpost_stack_pop(ctx->lo, ctx->os);
                      if (DEBUGLOAD)
                          xpost_object_dump(ret);
                      return ret;
                  } else {
                      ns = 1 + puff(ctx, s+1, NBUF-1, src, next, back);
                  }
                  if (ns == NBUF) error(limitcheck, "grok name exceeds buf");
                  s[ns] = '\0';
                  return xpost_object_cvlit(consname(ctx, s));
              }
    default: {
                 return xpost_object_cvx(consname(ctx, s));
             }
    }
}

/* read until a non-whitespace, non-comment char.
   "prime" the buffer.  */
static
int snip (context *ctx,
          char *buf,
          Xpost_Object *src,
          int (*next)(context *ctx, Xpost_Object *src))
{
    int c;
    do {
        c = next(ctx, src);
        if (c == '%') {
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
int puff (context *ctx,
          char *buf,
          int nbuf,
          Xpost_Object *src,
          int (*next)(context *ctx, Xpost_Object *src),
          void (*back)(context *ctx, int c, Xpost_Object *src))
{
    int c;
    char *s = buf;
    while (isreg(c = next(ctx, src))) {
        if (s-buf >= nbuf) return 0;
        *s++ = c;
    }
    if (!isspace(c) && c != EOF) back(ctx, c, src);
    return s-buf;
}


static
Xpost_Object toke (context *ctx,
             Xpost_Object *src,
             int (*next)(context *ctx, Xpost_Object *src),
             void (*back)(context *ctx, int c, Xpost_Object *src))
{
    char buf[NBUF] = "";
    int sta;  // status, and size
    Xpost_Object o;
    sta = snip(ctx, buf, src, next);
    if (!sta)
        return null;
    if (!isdel(*buf))
        sta += puff(ctx, buf+1, NBUF-1, src, next, back);
    o = grok(ctx, buf, sta, src, next, back);
    return o;
} 


static
int Fnext(context *ctx,
          Xpost_Object *F)
{
    return fgetc(filefile(ctx->lo, *F));
}
static
void Fback(context *ctx,
           int c,
           Xpost_Object *F)
{
    (void)ungetc(c, filefile(ctx->lo, *F));
}
static
void Ftoken (context *ctx,
             Xpost_Object F)
{
    Xpost_Object t;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Ftoken");
    t = toke(ctx, &F, Fnext, Fback);
    if (xpost_object_get_type(t) != nulltype) {
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
    } else {
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    }
}

static
int Snext(context *ctx,
          Xpost_Object *S)
{
    int ret;
    if (S->comp_.sz == 0) return EOF;
    ret = charstr(ctx, *S)[0];
    ++S->comp_.off;
    --S->comp_.sz;
    return ret;
}
static
void Sback(context *ctx,
           int c,
           Xpost_Object *S)
{
    --S->comp_.off;
    ++S->comp_.sz;
    charstr(ctx, *S)[0] = c;
}
static
void Stoken (context *ctx,
             Xpost_Object S)
{
    Xpost_Object t;
    t = toke(ctx, &S, Snext, Sback);
    if (xpost_object_get_type(t) != nulltype) {
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
    } else {
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    }
}

void initoptok(context *ctx,
               Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "token", Ftoken, 2, 1, filetype); INSTALL;
    op = consoper(ctx, "token", Stoken, 3, 1, stringtype); INSTALL;
    ctx->opcuts.token = op.mark_.padw;
}

