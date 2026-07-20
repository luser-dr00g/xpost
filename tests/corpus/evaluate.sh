#!/bin/sh
# Render each corpus through xpost and Ghostscript and report the
# per-page difference. A corpus whose directory is absent or empty is
# skipped, so this is never a build dependency. Ghostscript is used as
# the differential reference; read the difference as a lead, not a
# verdict (see README.md).
#
#   evaluate.sh                 evaluate every corpus present
#   evaluate.sh ghostscript     evaluate one
#   XPOST=/path/to/xpost evaluate.sh    use a specific build
#
set -u
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
work="$here/.work"
XPOST=${XPOST:-"$root/build/src/bin/xpost"}
GS=${GS:-gs}

for tool in "$XPOST" "$GS"; do
    command -v "$tool" >/dev/null 2>&1 || [ -x "$tool" ] || {
        echo "evaluate: missing $tool -- skipping all" >&2; exit 0; }
done
command -v compare >/dev/null 2>&1 || {
    echo "evaluate: ImageMagick 'compare' not found -- skipping all" >&2; exit 0; }

# device and metric for one page, by corpus and file name. The Adobe
# halftone and pattern-screen pages are bilevel; everything else is
# colour.
device_for() {   # corpus base -> "ppm" | "pbm"
    case "$1/$2" in
        adobe/ht_*|adobe/bb_1[2-5]) echo pbm;;
        *) echo ppm;;
    esac
}

evaluate_corpus() {
    corpus=$1
    dir="$here/$corpus"
    set -- "$dir"/*.ps "$dir"/*.eps
    have=0
    for p in "$@"; do [ -f "$p" ] && have=1; done
    if [ "$have" = 0 ]; then
        echo "$corpus: absent -- skipped (fetch.sh $corpus)"
        return
    fi
    echo "=== $corpus"
    mkdir -p "$work"
    for p in "$@"; do
        [ -f "$p" ] || continue
        b=$(basename "$p" | sed 's/\.[Pp][Ss]$//;s/\.[Ee][Pp][Ss]$//')
        dev=$(device_for "$corpus" "$b")
        gsdev=${dev}raw
        rm -f "$work"/g_*.* "$work"/x_*.*
        # an optional compatibility prelude, prepended to both engines so
        # the input stays identical; used where a corpus assumes operators
        # outside the language the reference provides as extensions
        src="$p"
        if [ -f "$here/$corpus/prelude" ]; then
            cat "$here/$corpus/prelude" "$p" > "$work/src.ps"
            src="$work/src.ps"
        fi
        "$GS" -q -sDEVICE=$gsdev -sPAPERSIZE=letter -r72 -dNOSAFER \
              -dBATCH -dNOPAUSE -o "$work/g_%d.$dev" "$src" >/dev/null 2>&1
        timeout 240 "$XPOST" -d $dev -o "$work/x_%d.$dev" "$src" \
                </dev/null >"$work/xlog" 2>&1
        xstatus=$?
        xerr=$(grep -m1 -oE 'Error: [a-zA-Z.]+' "$work/xlog" | sed 's/Error: //')
        # a signal death or a timeout is a hard regression, distinct from a
        # controlled PostScript error (which just yields no page, below)
        if [ "$xstatus" -ge 128 ]; then
            echo "  $b  XPOST CRASHED (signal $((xstatus - 128)))"; continue
        fi
        if [ "$xstatus" = 124 ]; then
            echo "  $b  XPOST TIMED OUT"; continue
        fi
        ng=$(ls "$work"/g_*.$dev 2>/dev/null | wc -l)
        nx=$(ls "$work"/x_*.$dev 2>/dev/null | wc -l)
        if [ "$ng" = 0 ]; then echo "  $b  reference produced no page"; continue; fi
        if [ "$nx" = 0 ]; then echo "  $b  XPOST FAILED${xerr:+: $xerr}"; continue; fi
        i=1
        while [ "$i" -le "$ng" ]; do
            gp="$work/g_$i.$dev"; xp="$work/x_$i.$dev"
            [ -f "$xp" ] || { echo "  $b p$i  no xpost page"; i=$((i+1)); continue; }
            if [ "$dev" = pbm ]; then
                convert "$gp" -resize 12.5% "$work/a.png" 2>/dev/null
                convert "$xp" -resize 12.5% "$work/b.png" 2>/dev/null
                m=$(compare -metric RMSE "$work/a.png" "$work/b.png" null: 2>&1 \
                    | grep -oE '\([0-9.]+\)' | tr -d '()')
                printf "  %-16s p%-2s  tintRMSE %s\n" "$b" "$i" "${m:-?}"
            else
                m=$(compare -metric AE -fuzz 5% "$gp" "$xp" null: 2>&1 | grep -oE '^[0-9]+')
                printf "  %-16s p%-2s  AE %s\n" "$b" "$i" "${m:-?}"
            fi
            i=$((i+1))
        done
    done
    rm -rf "$work"
}

for name in ${*:-ghostscript casselman bwipp adobe}; do
    evaluate_corpus "$name"
done
