#!/bin/sh
# Meson/make-check wrapper for the svgwrite device: render a known scene
# (fill, stroke, glyph outlines) through setpagedevice and require the
# structural landmarks of the produced document -- the svg root sized in
# points with a device-unit viewBox, the filled rectangle's path with its
# exact coordinates, a stroked path carrying the graphics-state attributes,
# at least one glyph outline path with curve commands, and a circle fill
# whose curves are preserved. A second page at
# 144dpi must report the same point size with a doubled viewBox.
#   $1  path to the built xpost binary
set -u
xpost=$1
# relative: the OutputFile paths are named inside the PS program the
# interpreter runs, and a native interpreter under a POSIX shell need not
# share the shell's view of an absolute path
tmp=svgwrite-$$
trap 'rm -rf "$tmp"' 0
mkdir -p "$tmp"
cat > "$tmp/t.ps" <<PSEOF
<< /OutputDevice /svgwrite /OutputFile ($tmp/a.svg) /PageSize [200 100] >> setpagedevice
0 0 1 setrgbcolor newpath 20 20 moveto 60 0 rlineto 0 40 rlineto -60 0 rlineto closepath fill
1 0 0 setrgbcolor 2 setlinewidth 1 setlinejoin newpath 100 20 moveto 40 30 rlineto 40 -30 rlineto stroke
0 setgray /Courier findfont 18 scalefont setfont 20 80 moveto (Og) show
0 1 0 setrgbcolor newpath 160 70 15 0 360 arc closepath fill
0 setgray 1 setlinewidth newpath 130 30 10 0 180 arc stroke
0 setgray newpath 10.12345 5 moveto 5 0 rlineto 0 2 rlineto -5 0 rlineto closepath fill
showpage
<< /HWResolution [144 144] /OutputFile ($tmp/b.svg) >> setpagedevice
0 0 1 setrgbcolor newpath 20 20 moveto 60 0 rlineto 0 40 rlineto -60 0 rlineto closepath fill
showpage
<< /OutputDevice /null >> setpagedevice
quit
PSEOF
"$xpost" -q -d null -o /dev/null "$tmp/t.ps" </dev/null >/dev/null 2>&1

fail() { echo "FAIL: $1"; exit 1; }
a=$tmp/a.svg; b=$tmp/b.svg
[ -s "$a" ] || fail "no output"
grep -q '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="200pt" height="100pt" viewBox="0 0 200 100">' "$a" || fail "svg root"
grep -q '<path fill="rgb(0%,0%,100%)" fill-rule="nonzero" d="M20 80L80 80L80 40L20 40Z"/>' "$a" || fail "filled rect path"
grep -q '<path fill="none" stroke="rgb(100%,0%,0%)" stroke-width="2" stroke-linecap="butt" stroke-linejoin="round" stroke-miterlimit="10" d="M100 80L140 50L180 80"/>' "$a" || fail "stroked path"
grep -q '<path fill="rgb(0%,0%,0%)" d="M[0-9.]* [0-9.]* C' "$a" || fail "glyph outline"
grep -q '<path fill="rgb(0%,100%,0%)" fill-rule="nonzero" d="M175 30C' "$a" || fail "curve-preserving circle fill"
grep -q 'stroke-width="1"[^>]*d="M140 70C' "$a" || fail "curve-preserving stroke"
grep -q 'd="M10.1235 95L15.1235 95L15.1235 93L10.1235 93Z"' "$a" || fail "four-decimal coordinates"
grep -q '</svg>' "$a" || fail "closing tag"
grep -q 'width="200pt" height="100pt" viewBox="0 0 400 200"' "$b" || fail "144dpi page in points"
grep -q 'd="M40 160L160 160L160 80L40 80Z"' "$b" || fail "144dpi coordinates"
echo SUCCESS
exit 0
