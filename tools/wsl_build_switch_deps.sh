#!/bin/bash
# Build the Switch libraries the xboxrecomp Switch layer needs, from sources cloned in
# /root/sw-deps (devkitPro/SDL switch-sdl-2.28, KhronosGroup/glslang 15.4.0), and install
# them into $DEVKITPRO/portlibs/switch. apt.devkitpro.org answers 403, so no pacman packages.
#   wsl.exe -u root bash /mnt/d/nx/IceAge2_Switch/tools/wsl_build_switch_deps.sh [sdl|glslang|all]
set -uo pipefail
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
PL=$DEVKITPRO/portlibs/switch
D=/root/sw-deps
WHAT=${1:-all}
TC=$DEVKITPRO/cmake/Switch.cmake
[ -f "$TC" ] || { echo "no $TC"; exit 1; }

if [ "$WHAT" = sdl ] || [ "$WHAT" = all ]; then
  echo "== SDL2 (switch)"
  # Video off: the Vulkan renderer (nv2a_vk) presents through NVK itself and SDL is only
  # audio (+ joystick); SDL's Switch video needs EGL from switch-mesa, which we do not have.
  cmake -S $D/sdl2-switch -B $D/build-sdl2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PL \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_PTHREADS=ON \
    > $D/sdl2-cmake.log 2>&1 || { tail -30 $D/sdl2-cmake.log; exit 1; }
  ninja -C $D/build-sdl2 -j4 > $D/sdl2-build.log 2>&1 || { tail -30 $D/sdl2-build.log; exit 1; }
  ninja -C $D/build-sdl2 install > $D/sdl2-install.log 2>&1 || { tail -20 $D/sdl2-install.log; exit 1; }
  ls -la $PL/lib/libSDL2.a $PL/include/SDL2/SDL.h
fi

if [ "$WHAT" = glslang ] || [ "$WHAT" = all ]; then
  echo "== glslang (switch)"
  cmake -S $D/glslang -B $D/build-glslang -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$D/glslang-switch \
    -DBUILD_SHARED_LIBS=OFF -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DBUILD_EXTERNAL=OFF \
    -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_HLSL=OFF -DENABLE_SPVREMAPPER=OFF \
    > $D/glslang-cmake.log 2>&1 || { tail -30 $D/glslang-cmake.log; exit 1; }
  ninja -C $D/build-glslang -j4 > $D/glslang-build.log 2>&1 || { tail -30 $D/glslang-build.log; exit 1; }
  ninja -C $D/build-glslang install > $D/glslang-install.log 2>&1 || { tail -20 $D/glslang-install.log; exit 1; }
  ls $D/glslang-switch/lib
fi
echo "== done"
