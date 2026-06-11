#!/bin/bash
set -e
cd "$(dirname "$0")/.."
gcc -O1 -Isrc -DVRAM='((unsigned long)(void*)g_vram_buf)' \
    -include tools/host_vram.h \
    tools/render_harness.c src/graphics.c src/level.c src/physics.c \
    -o /tmp/render_harness -lm
