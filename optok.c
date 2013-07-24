#include <ctype.h>
#include <errno.h> /* errno */
#include <limits.h>
#include <math.h>
#include <stdbool.h> /* ob.h:bool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strchr */

#include "err.h"
#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "f.h"
#include "op.h"
#include "nm.h"
#include "opar.h"
#include "opdi.h"
#include "optok.h"

enum { NBUF = BUFSIZ };

int puff (context *ctx, char *buf, int nbuf, object *src,
        int (*next)(context *ctx, object *src),
        void (*back)(context *ctx, int c, object *src));
object toke (context *ctx, object *src,
        int (*next)(context *ctx, object *src),
        void (*back)(context *ctx, int c, object *src));

// handles the 'true' case from `string token` in eval() //<-- Used to
object arrstrhandler;
void strhandler (context *ctx) {
    object post, any;
    any = pop(ctx->lo, ctx->os);
    post = pop(ctx->lo, ctx->os);
    push(ctx->lo, ctx->es, post);
    if (type(any) == arraytype)
        push(ctx->lo, ctx->os, any);
    else
        push(ctx->lo, ctx->es, any);
#if 0
    printf("strhandler: os: ");
    dumpstack(ctx->lo, ctx->os);
    printf("es: ");
    dumpstack(ctx->lo, ctx->es);
    puts("");
#endif
}

int ishash (int c) { return c == '#'; }
int isdot (int c) { return c == '.'; }
int ise (int c) { return strchr("eE", c) != NULL; }
int issign (int c) { return strchr("+-", c) != NULL; }
int isdel (int c) { return strchr("()[]<>{}/%", c) != NULL; }
int isreg (int c) { return (c!=EOF) && (!isspace(c)) && (!isdel(c)); }
//int isxdigit (int c) { return strchr("0123456789ABCDEFabcdef", c) != NULL; }

typedef struct { int (*pred)(int); int y, n; } test;

test fsm_dec[] = {
    /* 0 */ { issign,  1,  1 },
    /* 1 */ { isdigit, 2, -1 },
    /* 2 */ { isdigit, 2, -1 } };
int accept_dec(int i) { return i == 2; }

test fsm_rad[] = {
    /* 0 */ { isdigit, 1, -1 },
    /* 1 */ { isdigit, 1,  2 },
    /* 2 */ { ishash,  3, -1 },
    /* 3 */ { isalnum, 4, -1 },
    /* 4 */ { isalnum, 4, -1 } };
int accept_rad(int i) { return i == 4; }

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
int accept_real(int i) {
    switch (i) { //case 2:
        case 6: case 10: return true; default: return false; }
    //return (i & 3) == 2;  // 2, 6 == 2|4, 10 == 2|8
}

int fsm_check (char *s, int ns, test *fsm,
        int (*accept)(int final)) {
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

object grok (context *ctx, char *s, int ns, object *src,
        int (*next)(context *ctx, object *src),
        void (*back)(context *ctx, int c, object *src)) {
    if (ns == NBUF) error("limitcheck");
    s[ns] = '\0';  //fsm_check & consname  terminate on \0

    if (fsm_check(s, ns, fsm_dec, accept_dec)) {
        long num;
        num = strtol(s, NULL, 10);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
            error("limitcheck");
        return consint(num);
    }

    else if (fsm_check(s, ns, fsm_rad, accept_rad)) {
        long base, num;
        base = strtol(s, &s, 10);
        if (base > 36 || base < 2)
            error("limitcheck");
        num = strtol(s+1, NULL, base);
        if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
            error("limitcheck");
        return consint(num);
    }

    else if (fsm_check(s, ns, fsm_real, accept_real)) {
        double num;
        num = strtod(s, NULL);
        if ((num == HUGE_VAL || num == -HUGE_VAL) && errno==ERANGE)
            error("limitcheck");
        return consreal(num);
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
                      if (sp-s > NBUF) error("limitcheck");
                      else *sp++ = c;
                  }
                  return cvlit(consbst(ctx, sp-s, s));
              }

    case '<': {
                  int c;
                  char d, *x = "0123456789ABCDEF", *sp = s;
                  while (c = next(ctx, src), c != '>' && c != EOF) {
                      if (isspace(c)) continue;
                      if (isxdigit(c)) c = strchr(x, tolower(c)) - x;
                      else error("syntaxerror");
                      d = c << 4; // hi nib
                      while(isspace(c = next(ctx, src)))
                          /**/;
                      if (isxdigit(c))
                          c = strchr(x, tolower(c)) - x;
                      else if (c == '>') {
                          back(ctx, c, src); // pushback for next iter
                          c = 0;             // pretend it got a 0
                      } else error("syntaxerror");
                      d |= c;
                      if (sp-s > ns) error("limitcheck");
                      *sp++ = d;
                  }
                  return cvlit(consbst(ctx, sp-s, s));
              }

    case '>': {
                  int c;
                  if ((c = next(ctx, src)) == '>') {
                      return cvx(consname(ctx, ">>"));
                  } else error("syntaxerror");
              }

    case '{': { // This is the one part that makes it a recursive-descent parser
                  object tail;
                  tail = consname(ctx, "}");
                  push(ctx->lo, ctx->os, mark);
                  while (1) {
                      object t = toke(ctx, src, next, back);
                      if (objcmp(ctx, t, tail) == 0)
                          break;
                      push(ctx->lo, ctx->os, t);
                  }
                  arrtomark(ctx);  // ie. the /] operator
                  return cvx(pop(ctx->lo, ctx->os));
              }

    case '/': {
                  *s = next(ctx, src);
                  //ns = puff(ctx, s, NBUF, src, next, back);
                  if (ns && *s == '/') {
                      object ret;
                      ns = puff(ctx, s, NBUF, src, next, back);
                      if (ns == NBUF) error("limitcheck");
                      s[ns] = '\0';
                      //push(ctx->lo, ctx->os, cvx(consname(ctx, s)));
                      //opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
                      printf("\ntoken: loading immediate name %s\n", s);
                      Aload(ctx, cvx(consname(ctx, s)));
                      ret = pop(ctx->lo, ctx->os);
                      dumpobject(ret);
                      return ret;
                  } else {
                      ns = 1 + puff(ctx, s+1, NBUF-1, src, next, back);
                  }
                  if (ns == NBUF) error("limitcheck");
                  s[ns] = '\0';
                  return cvlit(consname(ctx, s));
              }
    default: {
                 return cvx(consname(ctx, s));
             }
    }
}

/* read until a non-whitespace, non-comment char.
   "prime" the buffer.  */
int snip (context *ctx, char *buf, object *src,
        int (*next)(context *ctx, object *src)) {
    int c;
    do {
        c = next(ctx, src);
        if (c == '%') {
            do {
                c = next(ctx, src);
            } while(c != '\n' && c != '\f' && c != EOF);
        }
    } while(c != EOF && isspace(c));
    if (c == EOF) return false;
    *buf = c;
    return 1; // true, and size of buffer
}

/* read in a token up to delimiter
   read into buf any regular characters,
   if we read one too many, put it back, unless whitespace. */
int puff (context *ctx, char *buf, int nbuf, object *src,
        int (*next)(context *ctx, object *src),
        void (*back)(context *ctx, int c, object *src)) {
    int c;
    char *s = buf;
    while (isreg(c = next(ctx, src))) {
        if (s-buf >= nbuf) return 0;
        *s++ = c;
    }
    if (!isspace(c) && c != EOF) back(ctx, c, src);
    return s-buf;
}


object toke (context *ctx, object *src,
        int (*next)(context *ctx, object *src),
        void (*back)(context *ctx, int c, object *src)) {
    char buf[NBUF] = "";
    int sta;  // status, and size
    object o;
    sta = snip(ctx, buf, src, next);
    if (!sta)
        return null;
    if (!isdel(*buf))
        sta += puff(ctx, buf+1, NBUF-1, src, next, back);
    o = grok(ctx, buf, sta, src, next, back);
    return o;
} 


int Fnext(context *ctx, object *F) {
    return fgetc(filefile(ctx->lo, *F));
}
void Fback(context *ctx, int c, object *F) {
    ungetc(c, filefile(ctx->lo, *F));
}
void Ftoken (context *ctx, object F) {
    object t;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    t = toke(ctx, &F, Fnext, Fback);
    if (type(t) != nulltype) {
        push(ctx->lo, ctx->os, t);
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        push(ctx->lo, ctx->os, consbool(false));
    }
}

int Snext(context *ctx, object *S) {
    int ret;
    if (S->comp_.sz == 0) return EOF;
    ret = charstr(ctx, *S)[0];
    ++S->comp_.off;
    --S->comp_.sz;
    return ret;
}
void Sback(context *ctx, int c, object *S) {
    --S->comp_.off;
    ++S->comp_.sz;
    charstr(ctx, *S)[0] = c;
}
void Stoken (context *ctx, object S) {
    object t;
    t = toke(ctx, &S, Snext, Sback);
    if (type(t) != nulltype) {
        push(ctx->lo, ctx->os, S);
        push(ctx->lo, ctx->os, t);
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        push(ctx->lo, ctx->os, consbool(false));
    }
}

#define N(t) cvx(consname(ctx, #t))
#define L(t) consint(t)

#define ARR(n) ar = cvx(consbar(ctx, n)); i = 0
#define ADD(x) barput(ctx, ar, i++, x)
//#define DEF(name) bdcput(ctx, td, N(name), ar)
#define ADDSUB(n) { object sar = consbar(ctx, n); { object ar = sar; int i = 0
#define ENDSUB } ADD(sar); }

void initoptok(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;
    object ar;
    int i;
    //object td;
    //td = consbdc(ctx, 20); // % tokedict

    op = consoper(ctx, "token", Ftoken, 2, 1, filetype); INSTALL;
    op = consoper(ctx, "token", Stoken, 3, 1, stringtype); INSTALL;

    ARR(1);
        ADD(cvx(consoper(ctx, "strhandler", strhandler, 0, 0)));
        bdcput(ctx, sd, N(strhandler), ar);
        arrstrhandler = ar;
}

