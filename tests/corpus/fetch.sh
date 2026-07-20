#!/bin/sh
# Populate the differential corpora. Each corpus lives in its own
# directory here; the programs it holds are fetched or copied from
# their own source, never committed (see README.md). Every corpus is
# best-effort and independent: one that cannot be obtained leaves the
# others alone.
#
#   fetch.sh                 populate every corpus it can
#   fetch.sh ghostscript     just one
#   BWIPP=/path/to/checkout fetch.sh bwipp
#
set -u
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

get() {   # url outfile
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --max-time 60 -o "$2" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -T 60 -O "$2" "$1"
    else
        echo "fetch: need curl or wget" >&2; return 1
    fi
}

fetch_ghostscript() {
    base="https://raw.githubusercontent.com/ArtifexSoftware/ghostpdl/master/examples"
    d="$here/ghostscript"; mkdir -p "$d"
    for f in alphabet.ps colorcir.ps doretree.ps escher.ps golfer.eps \
             grayalph.ps ridt91.eps snowflak.ps spots.ps tiger.eps \
             vasarely.ps waterfal.ps; do
        get "$base/$f" "$d/$f" && echo "  ghostscript/$f" || echo "  MISS ghostscript/$f"
    done
}

fetch_casselman() {
    base="https://personal.math.ubc.ca/~cass/graphics/manual/pdf"
    d="$here/casselman"; mkdir -p "$d"
    n=1
    while [ "$n" -le 15 ]; do
        get "$base/ch$n.ps" "$d/ch$n.ps" && echo "  casselman/ch$n.ps" \
            || { echo "  MISS casselman/ch$n.ps"; rm -f "$d/ch$n.ps"; }
        n=$((n + 1))
    done
}

fetch_bwipp() {
    # BWIPP is a local checkout, not a download: the barcode resource
    # is generated, so it is copied rather than vendored. The
    # monolithic resource becomes this corpus's prelude, prepended to
    # every example. The packaged flavour is preferred: it loads its
    # data through 125 ASCII85Decode filters, so it exercises the
    # decode-filter path the plain flavour does not.
    src=${BWIPP:-"$HOME/src/postscriptbarcode"}
    ex="$src/contrib/Examples"
    mono="$src/build/monolithic_package/barcode.ps"
    [ -f "$mono" ] || mono="$src/build/monolithic/barcode.ps"
    if [ ! -f "$mono" ] || [ ! -d "$ex" ]; then
        echo "  bwipp: no checkout at $src (set BWIPP=... ; build the monolithic resource)"
        return
    fi
    d="$here/bwipp"; mkdir -p "$d"
    cp "$mono" "$d/prelude" && echo "  bwipp/prelude ($(basename "$(dirname "$mono")")/barcode.ps)"
    for f in "$ex"/*.ps; do
        cp "$f" "$d/" && echo "  bwipp/$(basename "$f")"
    done
}

fetch_adobe() {
    echo "  adobe: not fetchable (Adobe copyright, no canonical download)."
    echo "         Place flat *.ps files under $here/adobe/ -- see README SOURCES."
}

for name in ${*:-ghostscript casselman bwipp adobe}; do
    echo "populating $name ..."
    case "$name" in
        ghostscript) fetch_ghostscript;;
        casselman)   fetch_casselman;;
        bwipp)       fetch_bwipp;;
        adobe)       fetch_adobe;;
        *)           echo "  unknown corpus: $name" >&2;;
    esac
done
