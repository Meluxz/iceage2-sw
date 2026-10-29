#!/bin/bash
# Copy the probe dumps of a play_probe.sh session out as PNG: logs/<name>/.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_play_pngs.sh <name>
L=/root/iceage2-play-log
OUT=/mnt/d/nx/IceAge2_Switch/logs/${1:-playprobe}
mkdir -p "$OUT"
for f in "$L"/*.bmp; do
  [ -f "$f" ] && python3 /mnt/d/nx/IceAge2_Switch/tools/bmp2png.py "$f" "$OUT/$(basename "${f%.bmp}").png"
done
grep -A11 '^\[PROBE\]' "$L/stderr.txt" > "$OUT/probe.txt"
ls "$OUT"
