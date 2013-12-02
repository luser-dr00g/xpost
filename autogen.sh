#!/bin/sh

autoreconf -vif

if [ -z "$NOCONFIGURE" ]; then
  exec ./configure -C "$@"
fi
