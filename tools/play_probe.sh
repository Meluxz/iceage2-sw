#!/bin/bash
# play.sh with RECOMP_GL_PROBE armed by a flag file: the player gets to the bug, then
#   wsl.exe -u root touch /root/iceage2-play-log/probe.flag
# probes pixel X,Y of the next frame (draws, textures, the frame itself).
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/play_probe.sh <x> <y>
L=/root/iceage2-play-log
mkdir -p "$L"; rm -f "$L"/probe.flag* "$L"/inline.flag "$L"/f*.bmp
export RECOMP_GL_PROBE="${1:-100},${2:-200},$L/probe.flag"
export RECOMP_GL_DUMP="$L/f,1000000"
export RECOMP_INLINE_DUMP="$L/inline.flag" RECOMP_INLINE_DUMP_VSIZE=24
exec bash /mnt/d/nx/IceAge2_Switch/tools/play.sh
