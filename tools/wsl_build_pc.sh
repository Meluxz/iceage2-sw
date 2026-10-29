#!/bin/bash
# Linux (WSL) build of iceage2-pc: GCC + SDL2 + OpenGL, toolkit ../xboxrecomp-b3 (from burnout3-sw).
# Smart App Control blocks running our own unsigned Windows exe, so the PC stage runs here.
# Build tree in WSL's own filesystem (fast); sources stay on D:.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_build_pc.sh [configure]
set -uo pipefail
SRC=/mnt/d/nx/IceAge2_Switch/iceage2-pc
B=${B:-/root/iceage2-linux-b3}   # fresh tree: the old one caches XBOXRECOMP_DIR=../xboxrecomp
if [ "${1:-}" = "configure" ] || [ ! -f "$B/build.ninja" ]; then
  cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit 1
fi
t0=$SECONDS
ninja -C "$B" -k 0 -j "${JOBS:-2}"
rc=$?
echo "== ninja rc=$rc in $(( (SECONDS - t0) / 60 )) min"
ls -la "$B/iceage2_recomp" 2>/dev/null
exit $rc
