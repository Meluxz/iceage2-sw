#!/bin/bash
# Build Ice Age 2 for the Switch (devkitA64 + libnx, Vulkan renderer on mesa-switch's NVK).
#   NVK_SDK=/path/to/nvk tools/build_switch.sh [configure]
#     -> build/switch/iceage2_recomp.nro, copied to out/switch/iceage2/iceage2.nro
# Needs: devkitPro (devkitA64, libnx, switch-tools), switch-sdl2 and glslang for the Switch
# (tools/build_switch_deps.sh), the NVK SDK (NVK_SDK: holds lib/libvulkan.a, include/vulkan)
# and src/recomp/gen from tools/regen.sh. Optional: assets/icon.jpg (256x256) as the hbmenu icon.
set -uo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
B=${B:-$R/build/switch}
NVK=${NVK_SDK:-$R/build/deps/nvk}
GLSLANG=${GLSLANG_DIR:-$R/build/deps/glslang-switch}
OUT=${OUT:-$R/out/switch/iceage2}

[ -n "$(ls "$R/src/recomp/gen"/*.c 2>/dev/null)" ] || { echo "no generated code: run tools/regen.sh first"; exit 1; }
[ -f "$NVK/lib/libvulkan.a" ] || { echo "no NVK SDK at $NVK (set NVK_SDK; docs/BUILDING.md)"; exit 1; }
[ -f "$GLSLANG/lib/cmake/glslang/glslangConfig.cmake" ] || { echo "no Switch glslang at $GLSLANG (tools/build_switch_deps.sh glslang)"; exit 1; }
[ -f "$DEVKITPRO/portlibs/switch/include/SDL2/SDL.h" ] || { echo "no switch SDL2 (tools/build_switch_deps.sh sdl)"; exit 1; }

if [ "${1:-}" = "configure" ] || [ ! -f "$B/build.ninja" ]; then
  extra=()
  # Tools from another devkitPro install (e.g. Windows tools from WSL): NACPTOOL=... ELF2NRO=...
  [ -n "${NACPTOOL:-}" ] && extra+=(-DNX_NACPTOOL_EXE="$NACPTOOL")
  [ -n "${ELF2NRO:-}" ] && extra+=(-DNX_ELF2NRO_EXE="$ELF2NRO")
  cmake -S "$R" -B "$B" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DNVK_SDK="$NVK" -Dglslang_DIR="$GLSLANG/lib/cmake/glslang" \
    "${extra[@]}" || exit 1
fi
t0=$SECONDS
ninja -C "$B" -k 0 -j "${JOBS:-2}"
rc=$?
echo "== ninja rc=$rc in $(( (SECONDS - t0) / 60 )) min"
if [ $rc -eq 0 ]; then
  mkdir -p "$OUT"
  cp "$B/iceage2_recomp.nro" "$OUT/iceage2.nro"
  ls -la "$OUT/iceage2.nro"
fi
exit $rc
