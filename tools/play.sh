#!/bin/bash
# Play the Linux build: tools/play.sh. Close the window to quit; the log goes to build/play.log.
# Game data: ICEAGE2_GAME_DIR, else game/ in the repository.
R="$(cd "$(dirname "$0")/.." && pwd)"
B=${B:-$R/build/linux}
export ICEAGE2_GAME_DIR=${ICEAGE2_GAME_DIR:-$R/game}
# FPS every 10 s in the log, on top of the window title.
export RECOMP_FPS_LOG=1
# WSLg's PulseAudio drains in uneven bursts: a 43 ms queue (8 blocks) ran dry and crackled.
export RECOMP_AUDIO_BLOCKS=${RECOMP_AUDIO_BLOCKS:-24}
# WSLg: its Wayland socket presents faster than X11.
[ -z "${SDL_VIDEODRIVER:-}" ] && [ -S "${XDG_RUNTIME_DIR:-/run/user/0}/${WAYLAND_DISPLAY:-wayland-0}" ] \
  && export SDL_VIDEODRIVER=wayland
cd "$B" && ./iceage2_recomp > "$R/build/play.log" 2>&1
