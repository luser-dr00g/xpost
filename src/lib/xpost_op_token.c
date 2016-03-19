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

enum { NBUF = 2 * BUFSIZ };

static
int puff (Xpost_Context *ctx,
          char *buf,
          int nbuf,
          Xpost_Object *src,
          int (*next)(Xpost_Context *ctx, Xpost_Object *src),
          void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src));
static
int toke (Xpost_Context *ctx,
             Xpost_Object *src,
             int (*next)(Xpost_Context *ctx, Xpost_Object *src),
             void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src),
             Xpost_Object *retval);

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
        if (sp - s > ns) break;
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
int grok (Xpost_Context *ctx,
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

    if (fsm_check(s, ns, fsm_dec, accept_dec)) {
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

    else if (fsm_check(s, ns, fsm_rad, accept_rad)) {
        long base, num;
        base = strtol(s, &s, 10);
        if (base > 36 || base < 2)
        {
            XPOST_LOG_ERR("bad radix");
            return limitcheck;
        }
        errno = 0;
        num = strtol(s + 1, NULL, base);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
        {
            XPOST_LOG_ERR("radixnumber out of range");
            return limitcheck;
        }
        //return xpost_int_cons(num);
        *retval = xpost_int_cons(num);
        return 0;
    }

    else if (fsm_check(s, ns, fsm_real, accept_real)) {
        double num;
        num = strtod(s, NULL);
        if ((num == HUGE_VAL || num == -HUGE_VAL) && errno==ERANGE)
        {
            XPOST_LOG_ERR("real out of range");
            return limitcheck;
        }
        //return xpost_real_cons(num);
        *retval = xpost_real_cons((real)num);
        return 0;
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

    case '<': {
                  int c;
                  char d, *x = "0123456789ABCDEF", *sp = s;
                  c = next(ctx, src);
                  if (c == '<') {
                      //return xpost_object_cvx(xpost_name_cons(ctx, "<<"));
                      *retval = xpost_object_cvx(xpost_name_cons(ctx, "<<"));
                      return 0;
                  }
                  back(ctx, c, src);
                  while (c = next(ctx, src), c != '>' && c != EOF) {
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
                      else if (c == '>') {
                          back(ctx, c, src); // pushback for next iter
                          c = 0;             // pretend it got a 0
                      } else
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

    case '>': {
                  int c;
                  if ((c = next(ctx, src)) == '>') {
                      //return xpost_object_cvx(xpost_name_cons(ctx, ">>"));
                      *retval = xpost_object_cvx(xpost_name_cons(ctx, ">>"));
                      return 0;
                  } else
                  {
                      XPOST_LOG_ERR("bare angle bracket");
                      return syntaxerror;
                  }
              }
              return unregistered; //not reached

    case '{': { // This is the one part that makes it a recursive-descent parser
                  int ret;
                  Xpost_Object tail;
                  tail = xpost_name_cons(ctx, "}");
                  xpost_stack_push(ctx->lo, ctx->os, mark);
                  while (1) {
                      Xpost_Object t;
                      ret = toke(ctx, src, next, back, &t);
                      //printf("grok: x?%d", xpost_object_is_exe(t));
                      if (ret)
                          return ret;
                      if ((xpost_object_get_type(t) == nametype)
                              && (xpost_dict_compare_objects(ctx, t, tail) == 0))
                          break;
                      xpost_stack_push(ctx->lo, ctx->os, t);
                  }
                  ret = xpost_op_array_to_mark(ctx);  // ie. the /] operator
                  if (ret)
                      return ret;
                  //return xpost_object_cvx(xpost_stack_pop(ctx->lo, ctx->os));
                  *retval = xpost_object_cvx(xpost_stack_pop(ctx->lo, ctx->os));
                  return 0;
              }

    case '/': {
                  *s = next(ctx, src);
                  //ns = puff(ctx, s, NBUF, src, next, back);
                  if (ns && *s == '/') {
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
                  } else {
                      if (isspace(*s)) {
                          ns = 0;
                      } else if (isdel(*s)) {
                          back(ctx, *s, src);
                          ns = 0;
                      } else {
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
    default: {
                 //return xpost_object_cvx(xpost_name_cons(ctx, s));
                 *retval = xpost_object_cvx(xpost_name_cons(ctx, s));
                 return 0;
             }
    }
}

/* read until a non-whitespace, non-comment char.
   "prime" the buffer.  */
static
int snip (Xpost_Context *ctx,
          char *buf,
          Xpost_Object *src,
          int (*next)(Xpost_Context *ctx, Xpost_Object *src))
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
int puff (Xpost_Context *ctx,
          char *buf,
          int nbuf,
          Xpost_Object *src,
          int (*next)(Xpost_Context *ctx, Xpost_Object *src),
          void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src))
{
    int c;
    char *s = buf;
    while (isreg(c = next(ctx, src))) {
        if (s - buf >= nbuf) return 0;
        *s++ = c;
    }
    if (!isspace(c) && c != EOF) back(ctx, c, src);
    return s - buf;
}


static
int toke (Xpost_Context *ctx,
             Xpost_Object *src,
             int (*next)(Xpost_Context *ctx, Xpost_Object *src),
             void (*back)(Xpost_Context *ctx, int c, Xpost_Object *src),
             Xpost_Object *retval)
{
    char buf[NBUF] = "";
    int sta;  // status, and size
    Xpost_Object o;
    int ret;

    sta = snip(ctx, buf, src, next);
    if (!sta)
    {
        *retval = null;
        return 0;
    }
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
    (void)ungetc(c, xpost_file_get_file_pointer(ctx->lo, *F));
}
static
int Ftoken (Xpost_Context *ctx,
             Xpost_Object F)
{
    Xpost_Object t;
    int ret;
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    ret = toke(ctx, &F, Fnext, Fback, &t);
    if (ret)
        return ret;
    if (xpost_object_get_type(t) != nulltype) {
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    } else {
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
    ret = xpost_string_get_pointer(ctx, *S)[0];
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
int Stoken (Xpost_Context *ctx,
             Xpost_Object S)
{
    Xpost_Object t;
    int ret;

    ret = toke(ctx, &S, Snext, Sback, &t);
    if (ret)
        return ret;
    if (xpost_object_get_type(t) != nulltype) {
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, t);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    } else {
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    }
    return 0;
}

int xpost_oper_init_token_ops (Xpost_Context *ctx,
               Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "token", (Xpost_Op_Func)Ftoken, 2, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "token", (Xpost_Op_Func)Stoken, 3, 1, stringtype);
    INSTALL;
    ctx->opcode_shortcuts.token = op.mark_.padw;
    return 0;
}
