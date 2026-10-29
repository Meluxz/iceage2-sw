#!/bin/bash
# Build Ice Age 2 for the Switch (devkitA64 + libnx in WSL, Vulkan renderer on mesa-switch's
# NVK) and stage the NRO in D:\nx\IceAge2_Switch\sd\switch\iceage2\.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_build_switch.sh [configure]
# Needs: /opt/devkitpro (devkitA64, libnx), SDL2 and glslang for the Switch
# (tools/wsl_build_switch_deps.sh), the NVK SDK from D:\nx\_spilne\mesa-sdk.
set -uo pipefail
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
SRC=/mnt/d/nx/IceAge2_Switch/iceage2-pc
B=${B:-/root/iceage2-switch-vk}
NVK=${NVK_SDK:-/root/sw-deps/nvk}
GLSLANG=${GLSLANG_DIR:-/root/sw-deps/glslang-switch}
SD=/mnt/d/nx/IceAge2_Switch/sd/switch/iceage2

# The NVK SDK lives on D: (shared with the FH2 port); a copy inside WSL links faster.
if [ ! -f "$NVK/lib/libvulkan.a" ]; then
  mkdir -p "$NVK"
  cp -r /mnt/d/nx/_spilne/mesa-sdk/opt/devkitpro/portlibs/switch/. "$NVK/"
fi
[ -f "$DEVKITPRO/portlibs/switch/include/SDL2/SDL.h" ] || { echo "no switch SDL2"; exit 1; }

if [ "${1:-}" = "configure" ] || [ ! -f "$B/build.ninja" ]; then
  cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DNVK_SDK="$NVK" -Dglslang_DIR="$GLSLANG/lib/cmake/glslang" \
    -DNX_NACPTOOL_EXE=/root/sw-deps/bin/nacptool -DNX_ELF2NRO_EXE=/root/sw-deps/bin/elf2nro \
    || exit 1
fi
t0=$SECONDS
ninja -C "$B" -k 0 -j "${JOBS:-2}"
rc=$?
echo "== ninja rc=$rc in $(( (SECONDS - t0) / 60 )) min"
if [ $rc -eq 0 ]; then
  mkdir -p "$SD"
  cp "$B/iceage2_recomp.nro" "$SD/iceage2.nro"
  ls -la "$SD/iceage2.nro"
fi
exit $rc
