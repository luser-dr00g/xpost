# Introduction #

**This page refers to Xpost2 which is no longer maintained, but available in the downloads section. Pdf listing of source : http://code.google.com/p/xpost/downloads/detail?name=xpost2.pdf ... zip : http://code.google.com/p/xpost/downloads/detail?name=xpost2g.zip***

Most postscript operators in xpost2 are implemented as C functions.

A few are actually procedures.


# Details #

Here is the current, ridiculous output from `xpost -h` showing the signatures (expected types) of all installed operators.

<pre>
503(1)10:50 PM:~ 0> xpost -h<br>
usage: xpost [ options ] [ files ] ...<br>
Xpost first loads init.ps and err.ps,then processes options,<br>
then performs a save,then executes files at save level 1.<br>
=== options ===<br>
(option letters are all case-insensitive)<br>
-?<br>
-h              print this message<br>
-c'fragment'    execute postscript code<br>
-sname[=string]<br>
-dname[=token]  define name in systemdict<br>
-q<br>
-dQUIET         suppress messages<br>
-nd             shorthand for -dDEVICE=nulldevice<br>
=== significant definitions ===<br>
-dNOPAUSE       do not wait for keypress in showpage<br>
-dBATCH         do not launch executive<br>
-dNODISPLAY     do not initialize graphics at all<br>
-dDEVICE=X11    set device to X11 window<br>
-dDEVICE=pdf    set device to pdf writer<br>
-dDEVICE=nulldevice   set device to in-memory image<br>
-dOUTFILE=filename    set filename for file-oriented device<br>
=== level 1 operators not (+ yet) implemented ===<br>
setscreen currentscreen<br>
strokepath reversepath+<br>
pathforall<br>
definefont+<br>
ashow+ widthshow+ awidthshow+ kshow+<br>
StandardEncoding<br>
=== installed operator signatures: ===<br>
-0-init-0-<br>
-0-breakhere-0-<br>
anytype -1-pop-0-<br>
anytype anytype -2-exch-2-<br>
anytype -1-dup-2-<br>
integertype -1-copy-0-<br>
arraytype arraytype -2-copy-1-<br>
dicttype dicttype -2-copy-1-<br>
stringtype stringtype -2-copy-1-<br>
integertype -1-index-1-<br>
integertype integertype -2-roll-0-<br>
-0-clear-0-<br>
-0-count-1-<br>
-0-cleartomark-0-<br>
-0-counttomark-1-<br>
integertype integertype -2-add-1-<br>
floattype floattype -2-add-1-<br>
floattype floattype -2-div-1-<br>
integertype integertype -2-idiv-1-<br>
integertype integertype -2-mod-1-<br>
integertype integertype -2-mul-1-<br>
floattype floattype -2-mul-1-<br>
integertype integertype -2-sub-1-<br>
floattype floattype -2-sub-1-<br>
integertype -1-abs-1-<br>
realtype -1-abs-1-<br>
integertype -1-neg-1-<br>
realtype -1-neg-1-<br>
integertype -1-ceiling-1-<br>
realtype -1-ceiling-1-<br>
integertype -1-floor-1-<br>
realtype -1-floor-1-<br>
integertype -1-round-1-<br>
realtype -1-round-1-<br>
integertype -1-truncate-1-<br>
realtype -1-truncate-1-<br>
floattype -1-sqrt-1-<br>
floattype floattype -2-atan-1-<br>
floattype -1-cos-1-<br>
floattype -1-sin-1-<br>
floattype floattype -2-exp-1-<br>
floattype -1-ln-1-<br>
floattype -1-log-1-<br>
-0-rand-1-<br>
integertype -1-srand-0-<br>
-0-rrand-1-<br>
integertype -1-array-1-<br>
arraytype -1-length-1-<br>
dicttype -1-length-1-<br>
nametype -1-length-1-<br>
stringtype -1-length-1-<br>
arraytype integertype -2-get-1-<br>
dicttype anytype -2-get-1-<br>
stringtype integertype -2-get-1-<br>
arraytype integertype anytype -3-put-0-<br>
dicttype anytype anytype -3-put-1-<br>
stringtype integertype integertype -3-put-0-<br>
arraytype integertype integertype -3-getinterval-1-<br>
stringtype integertype integertype -3-getinterval-1-<br>
arraytype integertype arraytype -3-putinterval-0-<br>
stringtype integertype stringtype -3-putinterval-0-<br>
arraytype -1-aload-1-<br>
arraytype -1-astore-1-<br>
arraytype proctype -2-forall-1-<br>
dicttype proctype -2-forall-2-<br>
stringtype proctype -2-forall-1-<br>
integertype -1-dict-1-<br>
dicttype -1-maxlength-1-<br>
dicttype -1-begin-0-<br>
-0-end-0-<br>
anytype anytype -2-def-0-<br>
anytype -1-load-1-<br>
anytype anytype -2-store-0-<br>
dicttype anytype -2-known-1-<br>
anytype -1-where-2-<br>
-0-currentdict-1-<br>
-0-countdictstack-1-<br>
arraytype -1-dictstack-1-<br>
integertype -1-string-1-<br>
stringtype stringtype -2-anchorsearch-3-<br>
stringtype stringtype -2-search-4-<br>
stringtype -1-token-2-<br>
filetype -1-token-2-<br>
integertype integertype -2-eq-1-<br>
floattype floattype -2-eq-1-<br>
anytype anytype -2-eq-1-<br>
integertype integertype -2-ne-1-<br>
floattype floattype -2-ne-1-<br>
anytype anytype -2-ne-1-<br>
integertype integertype -2-ge-1-<br>
floattype floattype -2-ge-1-<br>
stringtype stringtype -2-ge-1-<br>
integertype integertype -2-gt-1-<br>
floattype floattype -2-gt-1-<br>
stringtype stringtype -2-gt-1-<br>
integertype integertype -2-le-1-<br>
floattype floattype -2-le-1-<br>
stringtype stringtype -2-le-1-<br>
integertype integertype -2-lt-1-<br>
floattype floattype -2-lt-1-<br>
stringtype stringtype -2-lt-1-<br>
booleantype booleantype -2-and-1-<br>
integertype integertype -2-and-1-<br>
booleantype -1-not-1-<br>
integertype -1-not-1-<br>
booleantype booleantype -2-or-1-<br>
integertype integertype -2-or-1-<br>
booleantype booleantype -2-xor-1-<br>
integertype integertype -2-xor-1-<br>
integertype integertype -2-bitshift-1-<br>
anytype -1-exec-0-<br>
booleantype proctype -2-if-0-<br>
booleantype proctype proctype -3-ifelse-0-<br>
integertype integertype integertype proctype -4-for-4-<br>
floattype floattype floattype proctype -4-for-4-<br>
integertype proctype -2-repeat-0-<br>
proctype -1-loop-0-<br>
-0-exit-0-<br>
-0-countexecstack-1-<br>
arraytype -1-execstack-1-<br>
-0-quit-0-<br>
-0-stop-0-<br>
anytype -1-stopped-0-<br>
anytype -1-type-1-<br>
anytype -1-cvlit-1-<br>
anytype -1-cvx-1-<br>
anytype -1-xcheck-1-<br>
arraytype -1-executeonly-1-<br>
filetype -1-executeonly-1-<br>
stringtype -1-executeonly-1-<br>
arraytype -1-noaccess-1-<br>
dicttype -1-noaccess-1-<br>
filetype -1-noaccess-1-<br>
stringtype -1-noaccess-1-<br>
arraytype -1-readonly-1-<br>
dicttype -1-readonly-1-<br>
filetype -1-readonly-1-<br>
stringtype -1-readonly-1-<br>
arraytype -1-rcheck-1-<br>
dicttype -1-rcheck-1-<br>
filetype -1-rcheck-1-<br>
stringtype -1-rcheck-1-<br>
arraytype -1-wcheck-1-<br>
dicttype -1-wcheck-1-<br>
filetype -1-wcheck-1-<br>
stringtype -1-wcheck-1-<br>
realtype -1-cvi-1-<br>
stringtype -1-cvi-1-<br>
integertype -1-cvi-1-<br>
stringtype -1-cvn-1-<br>
integertype -1-cvr-1-<br>
stringtype -1-cvr-1-<br>
realtype -1-cvr-1-<br>
numbertype integertype stringtype -3-cvrs-1-<br>
integertype stringtype -2-cvs-1-<br>
realtype stringtype -2-cvs-1-<br>
booleantype stringtype -2-cvs-1-<br>
stringtype stringtype -2-cvs-1-<br>
nametype stringtype -2-cvs-1-<br>
operatortype stringtype -2-cvs-1-<br>
anytype stringtype -2-cvs-1-<br>
stringtype stringtype -2-file-1-<br>
filetype -1-closefile-0-<br>
filetype -1-read-2-<br>
filetype integertype -2-write-0-<br>
filetype stringtype -2-readhexstring-2-<br>
filetype stringtype -2-writehexstring-0-<br>
filetype stringtype -2-readstring-2-<br>
filetype stringtype -2-writestring-0-<br>
filetype stringtype -2-readline-2-<br>
filetype -1-bytesavailable-1-<br>
-0-flush-0-<br>
filetype -1-flushfile-0-<br>
filetype -1-resetfile-0-<br>
filetype -1-status-1-<br>
-0-currentfile-1-<br>
stringtype -1-print-0-<br>
booleantype -1-echo-0-<br>
-0-save-1-<br>
savetype -1-restore-0-<br>
-0-vmstatus-3-<br>
proctype -1-bind-1-<br>
-0-usertime-1-<br>
stringtype -1-getenv-1-<br>
-0-suspenderrdepth-0-<br>
-0-enableerrdepth-0-<br>
integertype integertype -2-initcanvas-0-<br>
integertype integertype -2-initimagecanvas-0-<br>
floattype floattype -2-initpdfcanvas-0-<br>
-0-exitcanvas-0-<br>
-0-gsave-0-<br>
-0-grestore-0-<br>
-0-grestoreall-0-<br>
floattype -1-setlinewidth-0-<br>
-0-currentlinewidth-1-<br>
integertype -1-setlinecap-0-<br>
-0-currentlinecap-1-<br>
integertype -1-setlinejoin-0-<br>
-0-currentlinejoin-1-<br>
floattype -1-setmiterlimit-0-<br>
-0-currentmiterlimit-1-<br>
arraytype floattype -2-setdash-0-<br>
-0-currentdash-2-<br>
floattype -1-setflat-0-<br>
-0-currentflat-1-<br>
floattype -1-setgray-0-<br>
-0-currentgray-1-<br>
floattype floattype floattype -3-setrgbcolor-0-<br>
-0-currentrgbcolor-3-<br>
floattype floattype floattype -3-sethsbcolor-0-<br>
-0-currenthsbcolor-3-<br>
proctype -1-settransfer-0-<br>
-0-currenttransfer-1-<br>
-0-matrix-1-<br>
-0-initmatrix-0-<br>
arraytype -1-identmatrix-1-<br>
arraytype -1-defaultmatrix-1-<br>
arraytype -1-currentmatrix-1-<br>
arraytype -1-setmatrix-0-<br>
floattype floattype -2-translate-0-<br>
floattype floattype arraytype -3-translate-1-<br>
floattype floattype -2-scale-0-<br>
floattype floattype arraytype -3-scale-0-<br>
floattype -1-rotate-0-<br>
floattype arraytype -2-rotate-0-<br>
arraytype -1-concat-0-<br>
arraytype arraytype arraytype -3-concatmatrix-1-<br>
floattype floattype -2-transform-2-<br>
floattype floattype arraytype -3-transform-2-<br>
floattype floattype -2-dtransform-2-<br>
floattype floattype arraytype -3-dtransform-2-<br>
floattype floattype -2-itransform-2-<br>
floattype floattype arraytype -3-itransform-2-<br>
floattype floattype -2-idtransform-2-<br>
floattype floattype arraytype -3-idtransform-2-<br>
arraytype arraytype -2-invertmatrix-1-<br>
-0-newpath-0-<br>
-0-currentpoint-2-<br>
floattype floattype -2-moveto-0-<br>
floattype floattype -2-rmoveto-0-<br>
floattype floattype -2-lineto-0-<br>
floattype floattype -2-rlineto-0-<br>
floattype floattype floattype floattype floattype -5-arc-0-<br>
floattype floattype floattype floattype floattype -5-arcn-0-<br>
floattype floattype floattype floattype floattype floattype -6-curveto-0-<br>
floattype floattype floattype floattype floattype floattype -6-rcurveto-0-<br>
-0-closepath-0-<br>
-0-flattenpath-0-<br>
stringtype booleantype -2-charpath-0-<br>
-0-clippath-0-<br>
-0-pathbbox-4-<br>
-0-initclip-0-<br>
-0-clip-0-<br>
-0-eoclip-0-<br>
-0-erasepage-0-<br>
-0-fill-0-<br>
-0-eofill-0-<br>
-0-stroke-0-<br>
integertype integertype integertype arraytype proctype -5-image-0-<br>
integertype integertype booleantype arraytype proctype -5-imagemask-0-<br>
-0-showpage-0-<br>
-0-copypage-0-<br>
-0-currentrasteropcode-1-<br>
integertype -1-setrasteropcode-0-<br>
nametype -1-loadfcfont-1-<br>
stringtype -1-loadfcfont-1-<br>
dicttype -1-setfont-0-<br>
-0-currentfont-1-<br>
stringtype -1-show-0-<br>
stringtype -1-stringwidth-2-<br>
270 signatures<br>
197 opcodes<br>
226 definitions in systemdict<br>
</pre>

And for an enumeration of "procedure" operators, here's the output from the command PS>`userdict {pop =} forall` (with %annotations):

<pre>
FontDirectory<br>
findfont<br>
scalefont<br>
makefont<br>
ashow     %stub<br>
widthshow    %stub<br>
awidthshow   %stub<br>
kshow     %stub<br>
cleardictstack<br>
selectfont<br>
rectstroke<br>
resources<br>
defineresource<br>
findresource<br>
currentglobal  %dummy<br>
setglobal     %dummy<br>
dicttomark<br>
bdef<br>
_K<br>
setcmykcolor<br>
quit<br>
prompt<br>
executive<br>
execdict<br>
<br>
</pre>