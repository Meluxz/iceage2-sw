#!/bin/bash
# Into the first level by pad script, run/jump there, and log fps + stale audio every 10 s.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_level_perf.sh <name> [secs] [VAR=value ...]
NAME=${1:-perf}; SECS=${2:-220}
shift 2 2>/dev/null
export RECOMP_PAD_SCRIPT="$(cat /mnt/d/nx/IceAge2_Switch/tools/pad_run_jump.txt)"
export RECOMP_FPS_LOG=1 RECOMP_APU_RING_STATS=1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-wayland}
for kv in "$@"; do export "$kv"; done
bash /mnt/d/nx/IceAge2_Switch/tools/wsl_shots.sh "$NAME" "$SECS" 1000000 >/dev/null 2>&1
grep -E '\[fps\]|ring voices' "/root/iceage2-trace/$NAME/stderr.txt" | tail -14
