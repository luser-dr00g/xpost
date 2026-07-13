#!/bin/sh
#
# Guard the single-opener invariant: fopen() must appear only inside
# xpost_diskfile_fopen(), so every disk file the interpreter opens passes
# one enforcement point. A new call elsewhere (for example a device that
# opens its own output) fails this check.
#
# Usage: check-fopen-funnel.sh <src/lib directory>

set -eu

libdir=${1:?usage: check-fopen-funnel.sh <src/lib directory>}

# Match a bare fopen( call token: \b keeps xpost_diskfile_fopen( out (the
# 'f' there follows '_', a word character), and printf("fopen\n") lacks the
# paren.
hits=$(grep -rnE '\bfopen\(' "$libdir" || true)

# The sole authorized call lives in xpost_file.c.
unauthorized=$(printf '%s\n' "$hits" | grep -v '/xpost_file\.c:' || true)

if [ -n "$unauthorized" ]; then
    echo "check-fopen-funnel: fopen() outside xpost_diskfile_fopen():" >&2
    printf '%s\n' "$unauthorized" >&2
    echo "Route disk opens through xpost_diskfile_fopen()." >&2
    exit 1
fi

# Exactly one call, and it is the one inside the opener.
count=$(printf '%s\n' "$hits" | grep -c '/xpost_file\.c:' || true)
if [ "$count" != "1" ]; then
    echo "check-fopen-funnel: expected exactly one fopen() in xpost_file.c, found $count:" >&2
    printf '%s\n' "$hits" | grep '/xpost_file\.c:' >&2
    exit 1
fi

echo "check-fopen-funnel: ok (single opener)"
