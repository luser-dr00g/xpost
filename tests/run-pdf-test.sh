#!/bin/sh
# Meson/make-check wrapper: write a PDF through the pdfwrite device and check
# it. A self-contained structural check always runs (PDF header, a fill
# operator, EOF trailer). When Ghostscript is available it is used as an
# independent oracle: the bounding box gs reads from our PDF must equal the box
# gs reads from the original drawing (a round-trip through the PDF).
#   $1  path to the built xpost binary
#   $2  path to the input drawing (a fill; e.g. bbox_test.ps)
set -u
xpost=$1
script=$2
# Temp files below are created only inside the Ghostscript-oracle blocks;
# predeclare them so the EXIT trap cleanup stays valid under set -u when a
# block is skipped (gs absent).
textps= textpdf= strokeps= strokepdf= ra= rb= infops= infopdf=
colorps= colorpdf= craster=
pdf=$(mktemp)
trap 'rm -f "$pdf"' EXIT

"$xpost" -q -d pdfwrite -o "$pdf" "$script" </dev/null >/dev/null 2>&1

# structural (no external dependency; the content stream may be compressed,
# so check the object structure rather than content-stream operators)
head -c 8 "$pdf" | grep -q '%PDF-1'    || { echo "FAIL: no PDF header";   exit 1; }
grep -q '/Type[ ]*/Page' "$pdf"        || { echo "FAIL: no page object";  exit 1; }
grep -q 'stream' "$pdf"                || { echo "FAIL: no content stream"; exit 1; }
tail -c 16 "$pdf" | grep -q '%%EOF'    || { echo "FAIL: no EOF trailer";  exit 1; }
echo "PDF structure OK"

# independent oracle: round-trip the bounding box through Ghostscript
if command -v gs >/dev/null 2>&1; then
    gsbb() { gs -q -dNOSAFER -dNOPAUSE -dBATCH -sDEVICE=bbox -o /dev/null "$1" 2>&1 \
             | grep '^%%BoundingBox:'; }
    a=$(gsbb "$pdf")
    b=$(gsbb "$script")
    echo "our PDF : $a"
    echo "original: $b"
    [ -n "$a" ] && [ "$a" = "$b" ] || { echo "FAIL: gs bbox round-trip mismatch"; exit 1; }
    echo "gs round-trip OK"

    # vector text: glyphs must reach the PDF as outlines that mark at the
    # pen position. The two interpreters may resolve the font name to
    # different faces, so compare the boxes coordinate-wise with a small
    # tolerance rather than exactly.
    textps=$(mktemp)
    textpdf=$(mktemp)
    trap 'rm -f "$pdf" "$textps" "$textpdf"' EXIT
    cat > "$textps" <<'EOF'
/Helvetica findfont 20 scalefont setfont
72 100 moveto (Vector Glyphs) show
showpage
EOF
    "$xpost" -q -d pdfwrite -o "$textpdf" "$textps" </dev/null >/dev/null 2>&1
    a=$(gsbb "$textpdf")
    b=$(gsbb "$textps")
    echo "our text PDF : $a"
    echo "original text: $b"
    if [ -n "$a" ] && [ -n "$b" ]; then
        set -- $(echo "$a" | tr -d '%:' | cut -d' ' -f2-)
        a1=$1 a2=$2 a3=$3 a4=$4
        set -- $(echo "$b" | tr -d '%:' | cut -d' ' -f2-)
        ok=1
        for pair in "$a1 $1" "$a2 $2" "$a3 $3" "$a4 $4"; do
            d=$(( ${pair% *} - ${pair#* } ))
            [ "$d" -ge -6 ] && [ "$d" -le 6 ] || ok=0
        done
        [ "$ok" = 1 ] || { echo "FAIL: text bbox diverges beyond face substitution"; exit 1; }
        echo "gs text round-trip OK"
    else
        echo "FAIL: text left no marks"; exit 1
    fi

    # glyph colour: text must mark in the current colour, not
    # unconditional black. White text over a black field must cut
    # visible holes: the dark-pixel count of the consumer's raster
    # falls measurably short of the untouched field.
    colorps=$(mktemp)
    colorpdf=$(mktemp)
    craster=$(mktemp)
    trap 'rm -f "$pdf" "$textps" "$textpdf" "$colorps" "$colorpdf" "$craster"' EXIT
    cat > "$colorps" <<'EOF'
0 setgray
20 40 moveto 300 40 lineto 300 100 lineto 20 100 lineto closepath fill
0 0 0 0 setcmykcolor
/Helvetica findfont 40 scalefont setfont
30 55 moveto (WHITE) show
showpage
EOF
    "$xpost" -q -d pdfwrite -o "$colorpdf" "$colorps" </dev/null >/dev/null 2>&1
    gs -q -dNOSAFER -dNOPAUSE -dBATCH -sDEVICE=pgmraw -g320x160 -r72 -o "$craster" "$colorpdf" 2>/dev/null
    dark=$(tail -c 51200 "$craster" | od -An -v -tu1 \
           | awk '{for(i=1;i<=NF;i++) if($i+0<128) n++} END{print n+0}')
    echo "glyph colour dark pixels: $dark"
    # the field alone is 280x60 = 16800 dark pixels: white glyphs must
    # carve out well over a thousand of them, black glyphs none
    [ "$dark" -ge 10000 ] && [ "$dark" -le 16000 ] \
        || { echo "FAIL: text did not mark in the current colour"; exit 1; }
    echo "gs glyph colour OK"

    # vector strokes: a bent polyline must reach the PDF as one path with
    # the requested width and the graphics state's join, not as separate
    # butt-capped segments at the consumer's default width. The defect is
    # sub-pixel at screen resolution, so rasterize our PDF and the original
    # drawing through gs at 288dpi and require near-identical rasters (a
    # small byte budget absorbs coordinate rounding at 1/100 point).
    strokeps=$(mktemp)
    strokepdf=$(mktemp)
    ra=$(mktemp)
    rb=$(mktemp)
    trap 'rm -f "$pdf" "$textps" "$textpdf" "$colorps" "$colorpdf" "$craster" "$strokeps" "$strokepdf" "$ra" "$rb"' EXIT
    cat > "$strokeps" <<'EOF'
0.75 setlinewidth
100 100 moveto 105 103.5 lineto 100 107 lineto
120 100 moveto 130 110 lineto 140 100 lineto
stroke
showpage
EOF
    "$xpost" -q -d pdfwrite -o "$strokepdf" "$strokeps" </dev/null >/dev/null 2>&1
    gsr() { gs -q -dNOSAFER -dNOPAUSE -dBATCH -sDEVICE=pbmraw -g2448x3168 -r288 -o "$2" "$1" 2>/dev/null; }
    gsr "$strokepdf" "$ra"
    gsr "$strokeps" "$rb"
    diffbytes=$(cmp -l "$ra" "$rb" 2>/dev/null | wc -l)
    echo "stroke raster diff: $diffbytes bytes"
    [ -s "$ra" ] && grep -q '[^\o000]' "$ra" || { echo "FAIL: stroke left no marks"; exit 1; }
    [ "$diffbytes" -le 8 ] || { echo "FAIL: stroked joints diverge from the original drawing"; exit 1; }
    echo "gs stroke round-trip OK"

    # document metadata: a DOCINFO pdfmark must land in the trailer's
    # Info dictionary, readable by the consumer
    infops=$(mktemp)
    infopdf=$(mktemp)
    trap 'rm -f "$pdf" "$textps" "$textpdf" "$colorps" "$colorpdf" "$craster" "$strokeps" "$strokepdf" "$ra" "$rb" "$infops" "$infopdf"' EXIT
    cat > "$infops" <<'EOF'
[ /Creator (pdf-device check) /DOCINFO pdfmark
100 100 moveto 200 100 lineto 200 200 lineto closepath fill
showpage
EOF
    "$xpost" -q -d pdfwrite -o "$infopdf" "$infops" </dev/null >/dev/null 2>&1
    grep -aq '/Info 5 0 R' "$infopdf" || { echo "FAIL: no Info reference in trailer"; exit 1; }
    creator=$(gs -q -dNODISPLAY -dPDFINFO -dBATCH -dNOPAUSE "$infopdf" </dev/null 2>&1 | grep '^Creator:')
    [ "$creator" = "Creator: pdf-device check" ] || { echo "FAIL: gs reads Creator as '$creator'"; exit 1; }
    echo "gs DOCINFO round-trip OK"
else
    echo "gs not found: skipping round-trip check"
fi

# process colour model: under /ProcessColorModel /DeviceCMYK every mark
# separates in CMYK -- default black (a DeviceGray source) and RGB black
# as pure K, explicit CMYK passed through, strokes as K, glyphs as k.
# Probed through the uncompressed accumulator, no consumer needed.
cmykps=$(mktemp)
cat > "$cmykps" <<'EOF'
<< /OutputDevice /pdfwrite /OutputFile (/dev/null) /PageSize [100 100] /ProcessColorModel /DeviceCMYK >> setpagedevice
newpath 10 10 moveto 20 0 rlineto 0 20 rlineto -20 0 rlineto closepath fill
1 0 0 setrgbcolor newpath 40 10 moveto 20 0 rlineto 0 20 rlineto -20 0 rlineto closepath fill
0 setgray 2 setlinewidth newpath 10 50 moveto 60 70 lineto stroke
/Courier findfont 12 scalefont setfont 10 80 moveto (K) show
/probe { % (needle) (name)  .  -
    exch DEVICE .pdfchunks 0 get exch search
    { pop pop pop (ok ) print print (\n) print }
    { pop (MISSING ) print print (\n) print } ifelse
} def
(0 0 0 1 k\n) (gray-black fill as pure K) probe
(0 1 1 0 k\n) (rgb red converted with undercolor removal) probe
(0 0 0 1 K\n) (stroke in CMYK) probe
( rg\n) (no RGB operators remain) exch DEVICE .pdfchunks 0 get exch search
    { pop pop pop (MISSING ) print print (\n) print }
    { pop (ok ) print print (\n) print } ifelse
showpage
<< /OutputDevice /null >> setpagedevice
quit
EOF
out=$("$xpost" -q -d null -o /dev/null "$cmykps" </dev/null 2>&1)
rm -f "$cmykps"
printf '%s\n' "$out" | grep -q 'MISSING' && { printf '%s\n' "$out" | grep MISSING; echo "FAIL: CMYK separation probes"; exit 1; }
n=$(printf '%s\n' "$out" | grep -c '^ok ')
[ "$n" = 4 ] || { echo "FAIL: expected 4 CMYK probes, saw $n"; exit 1; }
echo "CMYK process colour model OK"

# separation colour spaces: a [/Separation name alt tint] space set through
# setcolorspace/setcolor paints as /CS<i> cs t scn (CS/SCN for strokes) with
# the space preserved in the page's /ColorSpace resources and the tint
# transform embedded as a function -- Type 4 calculator source when the
# procedure stays within that operator set, sampled Type 0 otherwise (the
# second space's procedure reads a variable). Registration survives an
# intervening restore, and a gsave/grestore round-trip re-selects the
# separation after a process-colour interlude.
sepps=$(mktemp)
# A relative path resolves to the same file for the shell and for the
# interpreter, which is embedded in the program below and need not share the
# shell's view of an absolute path (e.g. a native binary under a POSIX shell).
seppdf=./sep-$$.pdf
trap 'rm -f "$pdf" "$textps" "$textpdf" "$strokeps" "$strokepdf" "$ra" "$rb" "$infops" "$infopdf" "$sepps" "$seppdf"' EXIT
cat > "$sepps" <<EOF
<< /OutputDevice /pdfwrite /OutputFile ($seppdf) /PageSize [100 100] >> setpagedevice
[/Separation (Spot A) /DeviceCMYK {dup 0 exch dup 0.5 mul exch 0.25 mul}] setcolorspace
0.8 setcolor
newpath 10 10 moveto 20 0 rlineto 0 20 rlineto -20 0 rlineto closepath fill
2 setlinewidth newpath 10 50 moveto 60 70 lineto stroke
gsave 0 setgray newpath 70 40 moveto 10 0 rlineto 0 10 rlineto -10 0 rlineto closepath fill grestore
newpath 40 10 moveto 10 0 rlineto 0 10 rlineto -10 0 rlineto closepath fill
/half 0.5 def
[/Separation /SpotB /DeviceGray {half mul 1 exch sub}] setcolorspace
save 1.0 setcolor newpath 50 50 moveto 20 0 rlineto 0 20 rlineto -20 0 rlineto closepath fill restore
/probe { % (needle) (name)  .  -
    exch DEVICE .pdfchunks 0 get exch search
    { pop pop pop (ok ) print print (\n) print }
    { pop (MISSING ) print print (\n) print } ifelse
} def
(/CS0 cs 0.8 scn\n) (fill in the separation) probe
(/CS0 CS 0.8 SCN\n) (stroke in the separation) probe
(0 0 0 rg\n) (process interlude inside gsave) probe
(/CS1 cs 1 scn\n) (registration survives restore) probe
showpage
<< /OutputDevice /null >> setpagedevice
quit
EOF
out=$("$xpost" -q -d null -o /dev/null "$sepps" </dev/null 2>&1)
rm -f "$sepps"
printf '%s\n' "$out" | grep -q 'MISSING' && { printf '%s\n' "$out" | grep MISSING; echo "FAIL: separation content probes"; exit 1; }
n=$(printf '%s\n' "$out" | grep -c '^ok ')
[ "$n" = 4 ] || { echo "FAIL: expected 4 separation probes, saw $n"; exit 1; }
sepdump() { echo "  seppdf=$seppdf ($(wc -c < "$seppdf" 2>/dev/null) bytes)"; grep -an 'Separation\|FunctionType\|0 obj' "$seppdf" 2>/dev/null | head -20; }
grep -aq '/CS0 \[/Separation /Spot#20A /DeviceCMYK 5 0 R\]' "$seppdf" || { echo "FAIL: no escaped Spot A colour space resource"; sepdump; exit 1; }
grep -aq '/CS1 \[/Separation /SpotB /DeviceGray 6 0 R\]' "$seppdf"   || { echo "FAIL: no SpotB colour space resource"; sepdump; exit 1; }
grep -aq '/FunctionType 4' "$seppdf" || { echo "FAIL: no Type 4 tint transform"; sepdump; exit 1; }
grep -aq '/FunctionType 0' "$seppdf" || { echo "FAIL: no sampled Type 0 tint transform"; sepdump; exit 1; }
echo "separation colour spaces OK"

# independent oracle: a separating consumer must image each separation as
# its own plate, named as given
if command -v gs >/dev/null 2>&1; then
    platedir=$(mktemp -d)
    gs -q -dNOSAFER -dNOPAUSE -dBATCH -sDEVICE=tiffsep -o "$platedir/p%d.tif" "$seppdf" >/dev/null 2>&1
    [ -f "$platedir/p1(Spot A).tif" ] || { ls "$platedir"; rm -rf "$platedir"; echo "FAIL: no Spot A plate"; exit 1; }
    [ -f "$platedir/p1(SpotB).tif" ]  || { ls "$platedir"; rm -rf "$platedir"; echo "FAIL: no SpotB plate"; exit 1; }
    rm -rf "$platedir"
    echo "gs separation plates OK"
fi
