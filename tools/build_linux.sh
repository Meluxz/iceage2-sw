#!/bin/bash
# Linux (or WSL) build: GCC + SDL2 + OpenGL.
#   tools/build_linux.sh [configure]       -> build/linux/iceage2_recomp
# Needs: cmake ninja-build gcc libsdl2-dev libgl1-mesa-dev libepoxy-dev, and src/recomp/gen
# from tools/regen.sh. The generated chunks are big: ~1.5 GB of RAM per compiler (JOBS).
set -uo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
B=${B:-$R/build/linux}
[ -n "$(ls "$R/src/recomp/gen"/*.c 2>/dev/null)" ] || { echo "no generated code: run tools/regen.sh first"; exit 1; }
if [ "${1:-}" = "configure" ] || [ ! -f "$B/build.ninja" ]; then
  cmake -S "$R" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit 1
fi
t0=$SECONDS
ninja -C "$B" -k 0 -j "${JOBS:-2}"
rc=$?
echo "== ninja rc=$rc in $(( (SECONDS - t0) / 60 )) min"
ls -la "$B/iceage2_recomp" 2>/dev/null
exit $rc
