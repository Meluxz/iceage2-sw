#!/bin/bash
# Build the Switch libraries the Vulkan build needs and that devkitPro does not package:
#   - SDL2 for Switch (devkitPro/SDL, branch switch-sdl-2.28), audio + joystick only,
#     installed into $DEVKITPRO/portlibs/switch
#   - glslang 15.4.0 (KhronosGroup/glslang) for the Switch, installed into $DEPS/glslang-switch
#   tools/build_switch_deps.sh [sdl|glslang|all]
# If switch-sdl2 is already installed with dkp-pacman, skip "sdl".
# NVK (mesa-switch) is separate: see docs/BUILDING.md.
set -uo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
PL=$DEVKITPRO/portlibs/switch
D=${DEPS:-$R/build/deps}
WHAT=${1:-all}
TC=$DEVKITPRO/cmake/Switch.cmake
[ -f "$TC" ] || { echo "no $TC: install devkitPro (devkitA64, libnx) first"; exit 1; }
mkdir -p "$D"

if [ "$WHAT" = sdl ] || [ "$WHAT" = all ]; then
  echo "== SDL2 (switch)"
  [ -d "$D/sdl2-switch" ] || git clone --depth 1 -b switch-sdl-2.28 https://github.com/devkitPro/SDL.git "$D/sdl2-switch" || exit 1
  # Video off: the Vulkan renderer presents through NVK itself; SDL is only audio (+ joystick).
  cmake -S "$D/sdl2-switch" -B "$D/build-sdl2" -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PL \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_PTHREADS=ON \
    > "$D/sdl2-cmake.log" 2>&1 || { tail -30 "$D/sdl2-cmake.log"; exit 1; }
  ninja -C "$D/build-sdl2" -j4 > "$D/sdl2-build.log" 2>&1 || { tail -30 "$D/sdl2-build.log"; exit 1; }
  ninja -C "$D/build-sdl2" install > "$D/sdl2-install.log" 2>&1 || { tail -20 "$D/sdl2-install.log"; exit 1; }
  ls -la $PL/lib/libSDL2.a $PL/include/SDL2/SDL.h
fi

if [ "$WHAT" = glslang ] || [ "$WHAT" = all ]; then
  echo "== glslang (switch)"
  [ -d "$D/glslang" ] || git clone --depth 1 -b 15.4.0 https://github.com/KhronosGroup/glslang.git "$D/glslang" || exit 1
  cmake -S "$D/glslang" -B "$D/build-glslang" -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$D/glslang-switch" \
    -DBUILD_SHARED_LIBS=OFF -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DBUILD_EXTERNAL=OFF \
    -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_HLSL=OFF -DENABLE_SPVREMAPPER=OFF \
    > "$D/glslang-cmake.log" 2>&1 || { tail -30 "$D/glslang-cmake.log"; exit 1; }
  ninja -C "$D/build-glslang" -j4 > "$D/glslang-build.log" 2>&1 || { tail -30 "$D/glslang-build.log"; exit 1; }
  ninja -C "$D/build-glslang" install > "$D/glslang-install.log" 2>&1 || { tail -20 "$D/glslang-install.log"; exit 1; }
  ls "$D/glslang-switch/lib"
fi
echo "== done"
