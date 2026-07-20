#!/bin/sh
# Meson wrapper for the differential corpus. It runs evaluate.sh over whatever
# corpora have been fetched into place and reports:
#   - SKIP (exit 77) when there is nothing to run: no corpus is present, or the
#     comparison tools it needs (Ghostscript, ImageMagick compare) are absent. The corpus
#     is thus never a build-time dependency -- populate it with fetch.sh to make
#     this test do its work.
#   - FAIL (exit 1) only when xpost crashes or hangs on a corpus program. A
#     rendering difference is a lead, not a verdict (see README.md), so it is
#     reported but does not fail the test; a signal death or a timeout is an
#     unambiguous regression and does.
#   - PASS (exit 0) otherwise, with the per-page differences left in the log for
#     inspection (meson test corpus -v, or meson-logs/testlog.txt).
#   $1  path to the built xpost binary (optional; evaluate.sh finds one itself)
set -u
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ "${1:-}" ] && XPOST=$1 && export XPOST

command -v gs >/dev/null 2>&1 || {
    echo "corpus: Ghostscript not found -- skipping"; exit 77; }
command -v compare >/dev/null 2>&1 || {
    echo "corpus: ImageMagick 'compare' not found -- skipping"; exit 77; }

have=0
for d in "$here"/*/; do
    for p in "$d"*.ps "$d"*.eps; do
        [ -f "$p" ] && { have=1; break 2; }
    done
done
[ "$have" = 1 ] || {
    echo "corpus: no corpus present -- run tests/corpus/fetch.sh, then re-run. Skipping."
    exit 77; }

out=$("$here/evaluate.sh" 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -Eq 'XPOST (CRASHED|TIMED OUT)' && {
    echo "corpus: xpost crashed or hung on a program -- see above"; exit 1; }
exit 0
