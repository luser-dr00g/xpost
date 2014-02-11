#!/bin/sh

valgrind -q --leak-check=full --track-origins=yes --suppressions=glibc.supp --log-file=.valgrind-log "$@"
result="$?"

# Valgrind should generate no error messages

log_contents="`cat .valgrind-log`"

if [ "$log_contents" != "" ]; then
        cat .valgrind-log >&2
        result=1
fi

rm -f .valgrind-log

exit $result
