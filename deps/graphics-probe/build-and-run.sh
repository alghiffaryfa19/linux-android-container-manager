#!/bin/sh
set -eu

OUT=${OUT:-/tmp/hybris-gralloc-probe}
CC=${CC:-cc}

$CC -std=c11 -O2 -Wall -Wextra -Werror \
    hybris_gralloc_probe.c -o "$OUT" \
    -lEGL -lGLESv2

export EGL_PLATFORM=${EGL_PLATFORM:-null}
export HYBRIS_EGLPLATFORM=${HYBRIS_EGLPLATFORM:-null}
exec "$OUT"
