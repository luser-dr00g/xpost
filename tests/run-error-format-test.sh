#!/bin/sh
# Meson/make-check wrapper: a top-level error in a job reports in the standard
# Adobe form -- the error line
#   %%[ Error: NAME; OffendingCommand: CMD ]%%
# (note the space before the closing bracket) followed by the flush notice
#   %%[ Flushing: rest of job (to end-of-file) will be ignored ]%%
# while a clean quit or a self-caught error reports neither. The two lines were
# verified byte-for-byte against Adobe Distiller.
#   $1  path to the built xpost binary
set -u
xpost=$1
tmp=${TMPDIR:-/tmp}/errfmt-$$
trap 'rm -f "$tmp".err.ps "$tmp".ok.ps "$tmp".caught.ps' 0

# 1. a top-level undefined error: the error line (with its trailing space)
#    and then the flush notice
printf 'mistypedname\n' > "$tmp".err.ps
out=$("$xpost" -q --no-sandbox -d null "$tmp".err.ps </dev/null 2>&1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -Fq '%%[ Error: undefined; OffendingCommand: mistypedname ]%%' || exit 1
printf '%s\n' "$out" | grep -Fq '%%[ Flushing: rest of job (to end-of-file) will be ignored ]%%' || exit 1

# 2. a clean job that quits normally: no flush notice
printf '(ok) = quit\n' > "$tmp".ok.ps
out=$("$xpost" -q --no-sandbox -d null "$tmp".ok.ps </dev/null 2>&1)
printf '%s\n' "$out" | grep -Fq 'Flushing' && exit 1

# 3. a job that catches its own error and completes: no flush notice
printf '{ oops } stopped pop (done) = quit\n' > "$tmp".caught.ps
out=$("$xpost" -q --no-sandbox -d null "$tmp".caught.ps </dev/null 2>&1)
printf '%s\n' "$out" | grep -Fq 'Flushing' && exit 1
printf '%s\n' "$out" | grep -Fq 'done' || exit 1

# 4. process exit status: an uncaught error is a failed job; a clean
#    job and a job that catches its own error succeed
"$xpost" -q --no-sandbox -d null "$tmp".err.ps </dev/null >/dev/null 2>&1 && exit 1
"$xpost" -q --no-sandbox -d null "$tmp".ok.ps </dev/null >/dev/null 2>&1 || exit 1
"$xpost" -q --no-sandbox -d null "$tmp".caught.ps </dev/null >/dev/null 2>&1 || exit 1

exit 0
