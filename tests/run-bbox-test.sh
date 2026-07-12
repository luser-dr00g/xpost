#!/bin/sh
# Meson/make-check wrapper: render a known fill through the bbox device and
# require the exact bounding box it reports. Exercises the -d bbox path end to
# end (device selection, fill accumulation, device->user y-flip), then the
# WhiteIsOpaque device key: by default painted white does not contribute to
# the box (crop semantics); with /WhiteIsOpaque true it does.
#   $1  path to the built xpost binary
#   $2  path to bbox_test.ps
set -u
xpost=$1
script=$2
expect='%%BoundingBox: 10 10 50 60'
out=$("$xpost" -q -d bbox -o /dev/null "$script" </dev/null 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx "$expect" || exit 1

tmp=${TMPDIR:-/tmp}/bbox-wio-$$.ps
trap 'rm -f "$tmp"' 0
cat > "$tmp" <<'PSEOF'
<< /OutputDevice /bbox /PageSize [200 200] /HWResolution [72 72] >> setpagedevice
0 setgray newpath 10 10 moveto 40 0 rlineto 0 40 rlineto -40 0 rlineto closepath fill
1 setgray newpath 100 100 moveto 50 0 rlineto 0 50 rlineto -50 0 rlineto closepath fill
showpage
<< /WhiteIsOpaque true >> setpagedevice
0 setgray newpath 10 10 moveto 40 0 rlineto 0 40 rlineto -40 0 rlineto closepath fill
1 setgray newpath 100 100 moveto 50 0 rlineto 0 50 rlineto -50 0 rlineto closepath fill
showpage
quit
PSEOF
out=$("$xpost" -q -d null -o /dev/null "$tmp" </dev/null 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '%%BoundingBox: 10 10 50 50' || exit 1
printf '%s\n' "$out" | grep -q '%%BoundingBox: 10 10 150 150' || exit 1
exit 0
