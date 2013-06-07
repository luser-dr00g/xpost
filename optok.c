/* systemdict<<token
   definition of the scanner procedures.

   For better or worse, many functions were named
   according to the metaphor of the evil vice *smoking*. 
   
       token (considered as germanized verb infinitive of "toke")
         1. snip (the tip of the cigar)
         2. puff (and light of course)
         3. grok (analyze and interpret
            3.a,b,c. if (check (fsm_dec,fsm_rad,fsm_real) interpretation
            3.d. lookup first char in grok_dict for interpretation
                3.d.1. ( string )
                3.d.2. < hexstring > | <<
                3.d.3. >>
                3.d.4. /literal-name
                3.d.5. default: executable-name (including [])

   Following the metaphor of metabolism could have been
   far worse. Consider, "masticate", "lick", "sniff"?

   */
#include <stdbool.h> /* ob.h:bool */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "nm.h"

void initoptok(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;
    object ar;
    int i;
    object td;
    td = consbdc(ctx, 20); // % tokedict, as in `/toke{//tokedict begin ...`

    /* "Alphabets" of the scanner */
    object alnum = consbst(ctx, 62,
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz");
    alnum = cvlit(alnum);
    object digit = arrgetinterval(alnum, 0, 10);
    object alpha = arrgetinterval(alnum, 10, 52);
    object upper = arrgetinterval(alnum, 10, 26);
    object lower = arrgetinterval(alnum, 36, 26);
    object u_l = consint('A' - 'a');  // ie. 'a' + u_l == 'A'

    /* constructor shortcuts */
#define N(t) cvx(consname(ctx, #t))
#define L(t) consint(t) // because I() looked weird, read: "Literal"

    /* proc (array) constructor macros */
#define ARR(n) ar = cvx(consbar(ctx, n)); i = 0
#define ADD(x) barput(ctx, ar, i++, x)
#define DEF(name) bdcput(ctx, td, N(name), ar)
#define ADDSUB(n) { object sar = consbar(ctx, n); { object ar = sar; int i = 0
#define ENDSUB } ADD(sar); }

    /* char str -indexof- idx
       return index of char in string
       /indexof { % char str . idx 
           -1 3 1 roll 0 exch {  % -1 c n s_n
               2 index eq {  % -1 c n
                   3 1 roll exit  % n -1 c
               } if  % -1 c n
               1 add  % -1 c n+1
           } forall pop pop  % n|-1
       } def */
    ARR(10);
        ADD(L(-1)); ADD(L(3)); ADD(L(1)); ADD(N(roll)); ADD(L(0)); ADD(N(exch));
        ADDSUB(7);
            ADD(L(2)); ADD(N(index)); ADD(N(eq));
            ADDSUB(4); ADD(L(3)); ADD(L(1)); ADD(N(roll)); ADD(N(exit)); ENDSUB;
            ADD(N(if));
            ADD(L(1)); ADD(N(add)); ENDSUB;
        ADD(N(forall)); ADD(N(pop)); ADD(N(pop));
        DEF(indexof);

    /* char str -within- bool
       test char is in string
       /within { % char str . bool
           indexof 0 ge
       } def
     */
    ARR(3); ADD(N(indexof)); ADD(L(0)); ADD(N(ge)); DEF(within);

    /* predicates for the finite state machines (automata)
       /israd { (#) 0 get eq } def
       /isalpha { alpha within } def
       /isdigit { digit within } def
       /isxdigit { %dup lower within { u-l add } if
           alnum indexof 16 lt } def
       /isupper { upper within } def
       /isalnum { dup isdigit { pop true }{ isalpha } ifelse } def
       /israddig { isalnum } def
       /isdot { (.) 0 get eq } def
       /ise { (eE) within } def
       /issign { (+-) within } def
       /isdel { (()<>[]{}/%) within } def
       /isspace { ( \t\n) within } def
       /isreg { dup isspace { pop false }{ isdel not } ifelse } def */
    ARR(2); ADD(L('#')); ADD(N(eq)); DEF(israd);
    ARR(2); ADD(alpha); ADD(N(within)); DEF(isalpha);
    ARR(2); ADD(digit); ADD(N(within)); DEF(isdigit);
    ARR(4); ADD(alnum); ADD(N(indexof)); ADD(L(16)); ADD(N(lt)); DEF(isxdigit);
    ARR(2); ADD(upper); ADD(N(within)); DEF(isupper);
    ARR(2); ADD(alnum); ADD(N(within)); DEF(isalnum);
    ARR(1); ADD(N(isalnum)); DEF(israddig);
    ARR(2); ADD(L('.')); ADD(N(eq)); DEF(isdot);
    ARR(2); ADD(consbst(ctx, 2, "eE")); ADD(N(within)); DEF(ise);
    ARR(2); ADD(consbst(ctx, 2, "+-")); ADD(N(within)); DEF(issign);
    ARR(2); ADD(consbst(ctx, 10, "()<>[]{}/%")); ADD(N(within)); DEF(isdel);
    ARR(2); ADD(consbst(ctx, 3, " \t\n")); ADD(N(within)); DEF(isspace);
    ARR(5); ADD(N(dup)); ADD(N(isspace));
        ADDSUB(2); ADD(N(pop)); ADD(N(false)); ENDSUB;
        ADDSUB(2); ADD(N(isdel)); ADD(N(not)); ENDSUB;
        ADD(N(ifelse)); DEF(isreg);

    /* automata states
       % the automaton type
       % [ predicate yes-transition no-transition ]

       % automaton to match a simple decimal number
       % /^[+-]?[0-9]+$/
       /fsm_dec [
           [ /issign   1  1 ] % 0
           [ /isdigit  2 -1 ] % 1
           [ /isdigit  2 -1 ] % 2
       ] def
       /accept_dec { 2 eq } def */
    ARR(3); ar = cvlit(ar);
        ADDSUB(3); ADD(N(issign));  ADD(L(1)); ADD(L(1));  ENDSUB; // 0
        ADDSUB(3); ADD(N(isdigit)); ADD(L(2)); ADD(L(-1)); ENDSUB; // 1
        ADDSUB(3); ADD(N(isdigit)); ADD(L(2)); ADD(L(-1)); ENDSUB; // 2
        object fsm_dec = ar;
        //DEF(fsm_dec);
    ARR(2); ADD(L(2)); ADD(N(eq)); 
        object accept_dec = ar;
        //DEF(accept_dec); //Don't define, just embed

    /*  % automaton to match a radix number
        % /^[0-9]+[#][a-Z0-9]+$/
        /fsm_rad [
            [ /isdigit   1 -1 ] % 0
            [ /isdigit   1  2 ] % 1
            [ /israd     3 -1 ] % 2
            [ /israddig  4 -1 ] % 3
            [ /israddig  4 -1 ] % 4
        ] def
        /accept_rad { 4 eq } def */
    ARR(5); ar = cvlit(ar);
        ADDSUB(3); ADD(N(isdigit));  ADD(L(1)); ADD(L(-1)); ENDSUB; // 0
        ADDSUB(3); ADD(N(isdigit));  ADD(L(1)); ADD(L(2));  ENDSUB; // 1
        ADDSUB(3); ADD(N(israd));    ADD(L(3)); ADD(L(-1)); ENDSUB; // 2
        ADDSUB(3); ADD(N(israddig)); ADD(L(4)); ADD(L(-1)); ENDSUB; // 3
        ADDSUB(3); ADD(N(israddig)); ADD(L(4)); ADD(L(-1)); ENDSUB; // 4
        object fsm_rad = ar;
        //DEF(fsm_rad);
    ARR(2); ADD(L(4)); ADD(N(eq));
        object accept_rad = ar;
        //DEF(accept_rad);

    /*  % automaton to match a real number
        % /^[+-]?(d+(.d*)?)|(d*.d+)([eE][+-]?d+)?$/
        %   where d = [0-9]
        /fsm_real [
            [ /issign   1   1 ] % 0
            [ /isdigit  2   4 ] % 1
            [ /isdigit  2   3 ] % 2
            [ /isdot    6   7 ] % 3
            [ /isdot    5  -1 ] % 4
            [ /isdigit  6  -1 ] % 5
            [ /isdigit  6   7 ] % 6
            [ /ise      8  -1 ] % 7
            [ /issign   9   9 ] % 8
            [ /isdigit  10 -1 ] % 9
            [ /isdigit  10 -1 ] % 10
        ] def
        /accept_real {
            dup 2 eq  % integer
            1 index 6 eq  % real
            2 index 10 eq  % exponent
            or or exch pop
        } def */
    ARR(11); ar = cvlit(ar);
        ADDSUB(3); ADD(N(issign));  ADD(L(1));  ADD(L(1));  ENDSUB; // 0
        ADDSUB(3); ADD(N(isdigit)); ADD(L(2));  ADD(L(4));  ENDSUB; // 1
        ADDSUB(3); ADD(N(isdigit)); ADD(L(2));  ADD(L(3));  ENDSUB; // 2
        ADDSUB(3); ADD(N(isdot));   ADD(L(6));  ADD(L(7));  ENDSUB; // 3
        ADDSUB(3); ADD(N(isdot));   ADD(L(5));  ADD(L(-1)); ENDSUB; // 4
        ADDSUB(3); ADD(N(isdigit)); ADD(L(6));  ADD(L(-1)); ENDSUB; // 5
        ADDSUB(3); ADD(N(isdigit)); ADD(L(6));  ADD(L(7));  ENDSUB; // 6
        ADDSUB(3); ADD(N(ise));     ADD(L(8));  ADD(L(-1)); ENDSUB; // 7
        ADDSUB(3); ADD(N(issign));  ADD(L(9));  ADD(L(9));  ENDSUB; // 8
        ADDSUB(3); ADD(N(isdigit)); ADD(L(10)); ADD(L(-1)); ENDSUB; // 9
        ADDSUB(3); ADD(N(isdigit)); ADD(L(10)); ADD(L(-1)); ENDSUB; // 10
        object fsm_real = ar;
        //DEF(fsm_real);
    ARR(15);
        ADD(N(dup)); ADD(L(2)); ADD(N(eq)); //integer
        ADD(L(1)); ADD(N(index)); ADD(L(6)); ADD(N(eq)); //real
        ADD(L(2)); ADD(N(index)); ADD(L(10)); ADD(N(eq)); //exponent
        ADD(N(or)); ADD(N(or)); ADD(N(exch)); ADD(N(pop));
        object accept_real = ar;
        //DEF(accept_real);

    /* str fsm accept-proc -check- bool
       execute the state machine against the string
       using accept-proc to interpret the final state
       /check { % str fsm accept  .  bool
           3 1 roll % acc str fsm
           0 exch 0 { % acc str n fsm sta
               2 copy get % acc str n fsm sta fsm[sta]
               5 3 roll % acc fsm sta fsm[sta] str n
               2 copy get % acc fsm sta fsm[sta] str n str[n]
               3 index 0 get % acc fsm sta fsm[sta] str n str[n] fsm[sta][0]
               cvx exec  % acc fsm sta fsm[sta] str n bool
               {
                   1 add 5 2 roll  % acc str n+1 fsm sta fsm[sta]
                   1 get exch pop  % acc str n+1 fsm sta'
               }{
                   5 2 roll        % acc str n fsm sta fsm[sta]
                   2 get exch pop  % acc str n fsm sta'
               } ifelse
               dup -1 eq {exit} if % acc str n(+1) fsm sta'
               2 index 4 index length ge {exit} if % acc str n(+1) fsm sta'
           } loop
           5 1 roll pop pop pop  % sta accept
           exec  % bool
       } def */
    ARR(15);
        ADD(L(3)); ADD(L(1)); ADD(N(roll));
        ADD(L(0)); ADD(N(exch)); ADD(L(0));
        ADDSUB(30);
            ADD(L(2)); ADD(N(copy)); ADD(N(get));
            ADD(L(5)); ADD(L(3)); ADD(N(roll));
            ADD(L(2)); ADD(N(copy)); ADD(N(get));
            ADD(L(3)); ADD(N(index)); ADD(L(0)); ADD(N(get));
            ADD(N(cvx)); ADD(N(exec));
            ADDSUB(9);
                ADD(L(1)); ADD(N(add)); ADD(L(5)); ADD(L(2)); ADD(N(roll));
                ADD(L(1)); ADD(N(get)); ADD(N(exch)); ADD(N(pop));
                ENDSUB;
            ADDSUB(7);
                ADD(L(5)); ADD(L(2)); ADD(N(roll));
                ADD(L(2)); ADD(N(get)); ADD(N(exch)); ADD(N(pop));
                ENDSUB;
            ADD(N(ifelse));
            ADD(N(dup)); ADD(L(-1)); ADD(N(eq));
                ADDSUB(1); ADD(N(exit)); ENDSUB; ADD(N(if));
            ADD(L(2)); ADD(N(index)); ADD(L(4)); ADD(N(index));
            ADD(N(length)); ADD(N(ge));
                ADDSUB(1); ADD(N(exit)); ENDSUB; ADD(N(if));
            ENDSUB;
            ADD(N(loop));
        ADD(L(5)); ADD(L(1)); ADD(N(roll));
        ADD(N(pop)); ADD(N(pop)); ADD(N(pop));
        ADD(N(exec));
        DEF(check);

    /* string radix -cvri- integer
        convert a string to integer using radix
        /cvri {   % string radix  .  num
            0 3 1 roll exch       %  0 base str
            dup 0 get issign {
                dup 0 get (-) 0 get eq 4 1 roll
                1 1 index length 1 sub getinterval
            }{ false 4 1 roll } ifelse   % bool sum base str i
            0 1 2 index length 1 sub {   % bool sum base str i
                2 copy get               % bool sum base str i s_i
                dup lower within { u-l add } if
                %dup =()=
                alnum indexof            % bool sum base str i digit
                5 4 roll 4 index mul     % bool base str i digit sum*base
                add                      % bool base str i sum=sum*base+digit
                4 1 roll pop             % bool sum base str
            } for
            pop pop                      % bool sum
            exch { neg } if              % num
        } def */
    ARR(26);
        ADD(L(0)); ADD(L(3)); ADD(L(1)); ADD(N(roll)); ADD(N(exch));
        ADD(N(dup)); ADD(L(0)); ADD(N(get)); ADD(N(issign));
        ADDSUB(15);
            ADD(N(dup)); ADD(L(0)); ADD(N(get));
            ADD(L('-')); ADD(N(eq)); ADD(L(4)); ADD(L(1)); ADD(N(roll));
            ADD(L(1)); ADD(L(1)); ADD(N(index));
            ADD(N(length)); ADD(L(1)); ADD(N(sub)); ADD(N(getinterval));
            ENDSUB;
        ADDSUB(4);
            ADD(N(false)); ADD(L(4)); ADD(L(1)); ADD(N(roll));
            ENDSUB;
        ADD(N(ifelse));
        ADD(L(0)); ADD(L(1)); ADD(L(2)); ADD(N(index));
        ADD(N(length)); ADD(L(1)); ADD(N(sub));
        ADDSUB(21);
            ADD(L(2)); ADD(N(copy)); ADD(N(get));
            ADD(N(dup)); ADD(N(lower)); ADD(N(within));
            ADDSUB(2); ADD(u_l); ADD(N(add)); ENDSUB; ADD(N(if));
            ADD(alnum); ADD(N(indexof));
            ADD(L(5)); ADD(L(4)); ADD(N(roll));
            ADD(L(4)); ADD(N(index)); ADD(N(mul)); ADD(N(add));
            ADD(L(4)); ADD(L(1)); ADD(N(roll)); ADD(N(pop));
            ENDSUB;
        ADD(N(for));
        ADD(N(pop)); ADD(N(pop));
        ADD(N(exch));
        ADDSUB(1); ADD(N(neg)); ENDSUB; ADD(N(if));
        DEF(cvri);

    /* string -radix- number
       interpret a string containing a radix number
       /radix {
           (#) search pop exch pop % (digits) (radix) % split
           10 cvri % (digits) radix
           cvri % num
       } def */
    ARR(8);
        ADD(consbst(ctx, 1, "#")); ADD(N(search));
        ADD(N(pop)); ADD(N(exch)); ADD(N(pop));
        ADD(L(10)); ADD(N(cvri));
        ADD(N(cvri));
        DEF(radix);

    /* grok_dict - postscript "switch" statement
       contains scanner actions for
       strings, hexstrings, array and dict names [ ] << >>,
       procs, and executable names. */
    object grok_dict;
    {
        object td = consbdc(ctx, 5); grok_dict = td;

        /* /default { cvn cvx } */
        ARR(2); // bareword: executable name
            ADD(N(cvn)); ADD(N(cvx));
            DEF(default);
    }

    /* string -grok- token
       interpret a string using fsm-acceptors and convertors
       /grok { 
           {
               dup fsm_dec //accept_dec check { 10 cvri exit } if
               dup fsm_rad //accept_rad check { radix exit } if
               dup fsm_real //accept_real check { cvr exit } if
               grok-dict 1 index 0 get
               2 copy known not { pop /default } if
               get exec
           exit } loop
       } def */
    ARR(2);
        ADDSUB(32);
            ADD(N(dup)); ADD(fsm_dec); ADD(accept_dec); ADD(N(check));
            ADDSUB(3);
                ADD(L(10)); ADD(N(cvri)); ADD(N(exit)); ENDSUB;
            ADD(N(if));
            ADD(N(dup)); ADD(fsm_rad); ADD(accept_rad); ADD(N(check));
            ADDSUB(2);
                ADD(N(radix)); ADD(N(exit)); ENDSUB;
            ADD(N(if));
            ADD(N(dup)); ADD(fsm_real); ADD(accept_real); ADD(N(check));
            ADDSUB(2);
                ADD(N(cvr)); ADD(N(exit)); ENDSUB;
            ADD(N(if));

            /* fallback. lookup first char in grok_dict */
            ADD(grok_dict); ADD(L(1)); ADD(N(index)); ADD(L(0)); ADD(N(get));
            ADD(L(2)); ADD(N(copy)); ADD(N(known)); ADD(N(not));
            ADDSUB(2); ADD(N(pop)); ADD(cvlit(N(default))); ENDSUB;
            ADD(N(if));
            ADD(N(get)); ADD(N(exec));

            ADD(N(exit));
            ENDSUB;
        ADD(N(loop));
        DEF(grok);

    /*  str -snip- str'
        trim initial whitespace from string
        /snip { % str . str'
            {
                dup length 0 eq { exit } if
                dup 0 get isspace not { exit } if
                1 1 index length 1 sub getinterval
            } loop
        } def */
    ARR(2);
        ADDSUB(19);
            ADD(N(dup)); ADD(N(length)); ADD(L(0)); ADD(N(eq));
            ADDSUB(1); ADD(N(exit)); ENDSUB; ADD(N(if));
            ADD(N(dup)); ADD(L(0)); ADD(N(get));
            ADD(N(isspace)); //n.b. #isspace not used in C namespace, no conflict with ctypes.h
            ADD(N(not));
            ADDSUB(1); ADD(N(exit)); ENDSUB; ADD(N(if));
            ADD(L(1)); ADD(L(1)); ADD(N(index)); ADD(N(length)); ADD(L(1)); ADD(N(sub));
            ADD(N(getinterval));
            ENDSUB;
        ADD(N(loop));
        DEF(snip);

    /*  puff
        read in a token up to delimiter
        /puff { % str . post pre
            1 1 index  % s 1 s
            1 1 index length 1 sub  % s 1 s 1 sN-1
            getinterval {  % s n s[n]
                isreg not { exit } if
                1 add  % s n+1
            } forall  % s n
            2 copy 0 exch getinterval  % s n s[0..n]
            3 1 roll  % s[0..n] s n
            1 index length  % s[0..n] s n sN
            1 index sub getinterval  % s[0..n) s[n..$)
            exch
       */
    ARR(28);
        ADD(L(1)); ADD(L(1)); ADD(N(index));
        ADD(L(1)); ADD(L(1)); ADD(N(index)); ADD(N(length)); ADD(L(1)); ADD(N(sub));
        ADD(N(getinterval));
        ADDSUB(6);
            ADD(N(isreg)); ADD(N(not));
            ADDSUB(1); ADD(N(exit)); ENDSUB;
            ADD(N(if));
            ADD(L(1)); ADD(N(add));
            ENDSUB;
        ADD(N(forall));
        ADD(L(2)); ADD(N(copy)); ADD(L(0)); ADD(N(exch)); ADD(N(getinterval));
        ADD(L(3)); ADD(L(1)); ADD(N(roll)); 
        ADD(L(1)); ADD(N(index)); ADD(N(length));
        ADD(L(1)); ADD(N(index)); ADD(N(sub)); ADD(N(getinterval));
        ADD(N(exch));
        DEF(puff);

    /*
        /toke {
            tokedict begin
            snip
            dup length 0 eq { false }{
                dup 0 get
                isdel not {
                    puff
                }{
                    dup 1 1 index length 1 sub getinterval
                    exch 0 1 getinterval
                } ifelse
                grok true
            } ifelse
            end
        } def */
    ARR(11);
        ADD(td); ADD(N(begin));
        ADD(N(snip));
        ADD(N(dup)); ADD(N(length)); ADD(L(0)); ADD(N(eq));
        ADDSUB(1); ADD(N(false)); ENDSUB;
        ADDSUB(10);
            ADD(N(dup)); ADD(L(0)); ADD(N(get));
            ADD(N(isdel)); ADD(N(not));
            ADDSUB(1); ADD(N(puff)); ENDSUB;
            ADDSUB(12);
                ADD(N(dup)); ADD(L(1)); ADD(L(1)); ADD(N(index));
                ADD(N(length)); ADD(L(1)); ADD(N(sub));
                ADD(N(getinterval)); ADD(N(exch));
                ADD(L(0)); ADD(L(1)); ADD(N(getinterval));
                ENDSUB;
            ADD(N(ifelse));
            ADD(N(grok)); ADD(N(true));
            ENDSUB;
        ADD(N(ifelse));
        ADD(N(end));
        DEF(toke);
        bdcput(ctx, sd, N(toke), ar);

    //op = consoper(ctx, "string", Istring, 1, 1, integertype); INSTALL;
}

