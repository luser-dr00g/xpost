#!/bin/sh
# Meson test wrapper: run the PLRM-example conformance suite (data/test.ps)
# in the freshly built interpreter and pass iff it reports SUCCESS -- i.e.
# the suite's internal failcount reached zero.
#   $1  path to the built xpost binary
#   $2  path to test.ps
set -u
xpost=$1
script=$2
out=$("$xpost" -q -d null "$script" </dev/null 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '^SUCCESS$'
