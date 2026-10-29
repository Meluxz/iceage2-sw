#!/bin/bash
# Play the Linux (WSL) build: no time limit. Close the window to quit.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/play.sh
# The log is written inside WSL's own filesystem and copied to D:\nx\IceAge2_Switch\logs\play
# when the game exits: writing it straight to /mnt/d (9P) stalled the game on every line
# (logs/profile01: 9 of 10 stack samples in write()).
B=${B:-/root/iceage2-linux-b3}
LOG=/root/iceage2-play-log
OUT=/mnt/d/nx/IceAge2_Switch/logs/play
mkdir -p "$LOG" "$OUT"
cd "$B"
export RECOMP_INPUT_DIAG=1
# Name every unresolved stub (code the disassembler never made a function) the first time it
# runs: a skipped piece of game logic looks like a freeze with sound still playing.
export RECOMP_STUB_LOG=1
# Draw on the GTX 1060 through WSL's d3d12 Mesa driver (/dev/dxg) instead of llvmpipe.
# ICEAGE2_SOFTWARE_GL=1 goes back to llvmpipe.
[ -z "${ICEAGE2_SOFTWARE_GL:-}" ] && export GALLIUM_DRIVER=d3d12
# WSLg's Wayland socket presents faster than X11 (XPutImage per frame): 28-29 fps in the
# first level instead of 25-26 (logs/perf04, perf05). SDL_VIDEODRIVER=x11 goes back.
[ -z "${SDL_VIDEODRIVER:-}" ] && [ -S "${XDG_RUNTIME_DIR:-/run/user/0}/${WAYLAND_DISPLAY:-wayland-0}" ] \
  && export SDL_VIDEODRIVER=wayland
# FPS every 10 s in the log ([fps] lines), on top of the window title.
export RECOMP_FPS_LOG=1
# WSLg's PulseAudio drains in uneven bursts: a 43 ms queue (8 blocks) ran dry and crackled.
# 24 blocks = 128 ms; sound lags the picture by that much, which a platformer hides.
export RECOMP_AUDIO_BLOCKS=${RECOMP_AUDIO_BLOCKS:-24}
./iceage2_recomp > "$LOG/stdout.txt" 2> "$LOG/stderr.txt"
rc=$?
cp "$LOG/stdout.txt" "$LOG/stderr.txt" "$OUT/"
exit $rc
