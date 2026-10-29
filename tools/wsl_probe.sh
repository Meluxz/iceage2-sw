#!/bin/bash
# Drive the game into the first level, then probe pixel x,y (RECOMP_GL_PROBE) in several frames,
# AT, AT+STEP, ... (COUNT of them); each probed frame is dumped too (f<frame>.png).
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_probe.sh <name> <at_s> <x> <y> [VAR=value ...]
#   env: COUNT (default 4), STEP seconds (default 10)
NAME=${1:-probe}; AT=${2:-130}; X=${3:-100}; Y=${4:-380}
shift 4 2>/dev/null
COUNT=${COUNT:-4}; STEP=${STEP:-10}
B=${B:-/root/iceage2-linux-b3}
LOG=/root/iceage2-trace/$NAME
OUT=/mnt/d/nx/IceAge2_Switch/logs/$NAME
FLAG=$LOG/probe.flag
rm -rf "$LOG" "$OUT"; mkdir -p "$LOG" "$OUT"
export GALLIUM_DRIVER=${GALLIUM_DRIVER:-d3d12}
export RECOMP_PAD_SCRIPT=${RECOMP_PAD_SCRIPT:-"20000:start:300,35000:start:300,50000:start:300,65000:a:300,75000:down:200,77000:down:200,79000:down:200,82000:a:300,88000:up:200,91000:a:300,105000:a:300"}
export RECOMP_GL_PROBE="$X,$Y,$FLAG"
export RECOMP_GL_DUMP="$LOG/f,1000000"
for kv in "$@"; do export "$kv"; done
cd "$B"
./iceage2_recomp > "$LOG/stdout.txt" 2> "$LOG/stderr.txt" &
PID=$!
sleep "$AT"
for i in $(seq 1 "$COUNT"); do
  touch "$FLAG"
  sleep "$STEP"
done
sleep 2
kill -9 "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
cp "$LOG/stdout.txt" "$LOG/stderr.txt" "$OUT/"
for f in $LOG/*.tex_*.bmp $LOG/*.surf_*.bmp; do [ -f "$f" ] && python3 /mnt/d/nx/IceAge2_Switch/tools/bmp2png.py "$f" "$OUT/$(basename "${f%.bmp}").png"; done
for f in "$LOG"/f*.bmp; do
  python3 /mnt/d/nx/IceAge2_Switch/tools/bmp2png.py "$f" "$OUT/$(basename "${f%.bmp}").png"
done
grep -A10 '^\[PROBE\]' "$LOG/stderr.txt" | grep -E '^\[PROBE\]|^        ' > "$OUT/probe.txt"
echo "probe lines: $(wc -l < "$OUT/probe.txt")"; ls "$OUT" | grep '^f'
