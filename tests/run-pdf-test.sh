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
    trap 'rm -f "$pdf" "$textps" "$textpdf" "$strokeps" "$strokepdf" "$ra" "$rb"' EXIT
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
    trap 'rm -f "$pdf" "$textps" "$textpdf" "$strokeps" "$strokepdf" "$ra" "$rb" "$infops" "$infopdf"' EXIT
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
