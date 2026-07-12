#!/bin/sh
# Meson wrapper: the pngalpha device must write RGBA where the erased
# page is transparent, marks are opaque, and an explicit white fill
# stays opaque (distinct from the page background); the png device must
# stay plain RGB. Colour types come from the IHDR; pixel semantics are
# checked when python3 is available.
#   $1  path to the built xpost binary
set -u
xpost=$1

ps=$(mktemp)
outa=$(mktemp)
outrgb=$(mktemp)
trap 'rm -f "$ps" "$outa" "$outrgb"' EXIT

cat > "$ps" <<'EOF'
newpath 20 20 moveto 100 20 lineto 100 60 lineto 20 60 lineto closepath fill
1 setgray newpath 120 20 moveto 200 20 lineto 200 60 lineto 120 60 lineto closepath fill
showpage
quit
EOF

"$xpost" -q -d pngalpha -o "$outa" "$ps" </dev/null >/dev/null 2>&1
"$xpost" -q -d png -o "$outrgb" "$ps" </dev/null >/dev/null 2>&1

# IHDR colour type: byte 25 of the file (2 = RGB, 6 = RGBA)
ct() { od -An -j25 -N1 -tu1 "$1" | tr -d ' '; }
[ "$(ct "$outa")" = 6 ]   || { echo "FAIL: pngalpha colour type $(ct "$outa"), want 6"; exit 1; }
[ "$(ct "$outrgb")" = 2 ] || { echo "FAIL: png colour type $(ct "$outrgb"), want 2"; exit 1; }
echo "colour types OK (RGBA=6, RGB=2)"

if command -v python3 >/dev/null 2>&1; then
    python3 - "$outa" <<'PYEOF'
import struct, sys, zlib
d = open(sys.argv[1],'rb').read()
pos, idat, meta = 8, b'', {}
while pos < len(d):
    ln, typ = struct.unpack('>I4s', d[pos:pos+8])
    if typ == b'IHDR':
        meta['w'], meta['h'] = struct.unpack('>II', d[pos+8:pos+16])
    if typ == b'IDAT':
        idat += d[pos+8:pos+8+ln]
    pos += 12 + ln
raw = zlib.decompress(idat)
W, H = meta['w'], meta['h']
stride = W*4
out, prev, i = bytearray(), bytearray(stride), 0
for y in range(H):
    f = raw[i]; i += 1
    line = bytearray(raw[i:i+stride]); i += stride
    for x in range(stride):
        a = line[x-4] if x>=4 else 0
        b = prev[x]
        c = prev[x-4] if x>=4 else 0
        if f==1: line[x]=(line[x]+a)&255
        elif f==2: line[x]=(line[x]+b)&255
        elif f==3: line[x]=(line[x]+(a+b)//2)&255
        elif f==4:
            pp=a+b-c; pa,pb,pc=abs(pp-a),abs(pp-b),abs(pp-c)
            line[x]=(line[x]+(a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)))&255
    out += line; prev = line
def pix(x,y):
    o=(y*W+x)*4; return tuple(out[o:o+4])
checks = [
    (pix(2,2)[3] == 0,               "erased page is transparent"),
    (pix(60,H-40) == (0,0,0,255),    "ink is opaque"),
    (pix(160,H-40) == (255,255,255,255), "an explicit white fill is opaque"),
]
bad = [msg for ok,msg in checks if not ok]
for m in bad: print("FAIL:", m)
sys.exit(1 if bad else 0)
PYEOF
    [ $? -eq 0 ] || exit 1
    echo "alpha semantics OK"
else
    echo "python3 not found: pixel semantics not checked"
fi
