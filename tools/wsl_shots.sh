#!/bin/bash
# Run the game for N seconds dumping a frame every K frames, copy all frames (PNG) out.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_shots.sh <name> <seconds> <every> [VAR=value ...]
NAME=${1:-shots}; SECS=${2:-120}; EVERY=${3:-100}
shift 3 2>/dev/null
B=${B:-/root/iceage2-linux-b3}
LOG=/root/iceage2-trace/$NAME
OUT=/mnt/d/nx/IceAge2_Switch/logs/$NAME
rm -rf "$LOG"; mkdir -p "$LOG" "$OUT"
export GALLIUM_DRIVER=${GALLIUM_DRIVER:-d3d12}
export RECOMP_GL_DUMP="$LOG/f,$EVERY"
for kv in "$@"; do export "$kv"; done
cd "$B"
timeout -s KILL "$SECS" ./iceage2_recomp > "$LOG/stdout.txt" 2> "$LOG/stderr.txt"
cp "$LOG/stdout.txt" "$LOG/stderr.txt" "$OUT/"
for f in "$LOG"/f*.bmp; do
  python3 /mnt/d/nx/IceAge2_Switch/tools/bmp2png.py "$f" "$OUT/$(basename "${f%.bmp}").png"
done
ls "$OUT" | head -100
