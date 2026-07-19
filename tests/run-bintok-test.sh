#!/bin/sh
# Meson wrapper: run the binary token conformance corpus through xpost and
# compare against the committed golden record. The golden is the conformance
# baseline (see binary_token_test.ps for how it was captured and the command
# to regenerate it), so this check is self-contained and needs no other tool.
#   $1  path to the built xpost binary
#   $2  path to binary_token_test.ps
#   $3  path to binary_token_test.expected
set -u
xpost=$1
corpus=$2
golden=$3

# scratch is named inside the PS program the interpreter runs, so keep it
# relative: a native interpreter under a POSIX shell need not share /tmp
scratch=bintok_scratch_$$
job=$(mktemp)
out=$(mktemp)
trap 'rm -f "$scratch" "$job" "$out"' EXIT

{ echo "/SCRATCH ($scratch) def"; cat "$corpus"; echo quit; } > "$job"

"$xpost" -q -d null "$job" </dev/null 2>/dev/null \
    | grep -v '^Xpost\|^Copyright\|WARRANTY\|COPYING\|^PS' > "$out"

# --strip-trailing-cr: the golden may be checked out with CRLF on a host that
# translates line endings, while the interpreter emits LF
if ! diff -u --strip-trailing-cr "$golden" "$out"; then
    echo "FAIL: xpost diverges from the binary token golden record"
    exit 1
fi
echo "xpost matches golden ($(wc -l < "$golden") lines)"
