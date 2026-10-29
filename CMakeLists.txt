cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Ice Age 2: The Meltdown (Xbox, NTSC-U) - Static Recompilation, PC build
#
# Copy this template to your game project root and customize:
#   1. Change the project name below
#   2. Set XBOXRECOMP_DIR to where you cloned xboxrecomp
#   3. Adjust source file lists as you add game-specific code
#   4. Add any game-specific compile definitions or link libraries
# ============================================================================

project(iceage2_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Release with a PDB: the VEH crash handler names the guest function (sub_XXXXXXXX) through
# dbghelp, which needs the .pdb beside the .exe. /OPT:REF,ICF keep the exe as small as without it.
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
#
# Option A: xboxrecomp as a subdirectory (simplest)
#   Place xboxrecomp/ next to your game project and use a relative path.
#
# Option B: find_package (if you install xboxrecomp system-wide)
#   find_package(xboxrecomp REQUIRED)
#
# xboxrecomp/: the toolkit as vendored in burnout3-sw (MIT), with the NV2A renderers (OpenGL,
# Vulkan), the MMIO-lifted device models and the Switch layer, plus this port's fixes. It must
# match the translator that generated src/recomp/gen (tools/regen.sh).
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")
# ICEAGE2_ABI_CHECK: log every lifted call that returns with ebx/esi/edi/esp not preserved
# (recomp_types.h RECOMP_ABI_CALL; the logger lives in the runtime, so both get the define).
# Slower; for bring-up.
option(ICEAGE2_ABI_CHECK "Check callee-saved registers on every lifted call" OFF)
if(ICEAGE2_ABI_CHECK)
    add_compile_definitions(RECOMP_ABI_CHECK)
endif()
# OpenGL renderer on Linux; Vulkan (NVK from mesa-switch, NVK_SDK) on the Switch
# (tools/wsl_build_switch.sh). There is no switch-mesa here, so the Switch build is Vulkan only.
if(NINTENDO_SWITCH)
    set(ICEAGE2_VULKAN_DEFAULT ON)
else()
    set(ICEAGE2_VULKAN_DEFAULT OFF)
endif()
option(ICEAGE2_VULKAN "Render with Vulkan (nv2a_vk) instead of OpenGL" ${ICEAGE2_VULKAN_DEFAULT})
set(XBOXRECOMP_VULKAN ${ICEAGE2_VULKAN} CACHE BOOL "" FORCE)

# Add xboxrecomp as a subdirectory -- this builds all 6 runtime libraries
# and provides the 'xboxrecomp' umbrella target to link against.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# ── Recompiled game executable ──────────────────────────────────

# Collect generated recompiled code.
# The recomp pipeline outputs files into src/recomp/gen/.
# With --split 1000, you get gen_0000.c, gen_0001.c, etc.
# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed files
# without a manual re-configure. A plain GLOB is evaluated once at configure
# time, so a newly generated file (an extra chunk, recomp_stubs_unresolved.c)
# is silently left out of the build and shows up as unresolved externals.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

# Core game sources
# main.c is Win32 (VEH, WinMain); main_posix.c is the same boot for Linux (WSL).
if(WIN32)
    set(ICEAGE2_MAIN src/main.c)
else()
    set(ICEAGE2_MAIN src/main_posix.c)
endif()

if(NINTENDO_SWITCH)
    # From burnout3-sw (MIT, src/LICENSE.burnout3-sw): log device, env file, exception
    # handler, SaltyNX frame-rate block.
    list(APPEND ICEAGE2_MAIN src/switch_nx.c src/switch_saltynx.c)
endif()

set(GAME_SOURCES
    ${ICEAGE2_MAIN}
    src/recomp_manual.c
    # Add game-specific sources here, e.g.:
    # src/asset_loader.c
    # src/custom_renderer.c
)

# The recomp pipeline also generates a dispatch header and function declarations.
# These are included by the generated .c files:
#   src/recomp/gen/recomp_funcs.h    - extern declarations for all recompiled functions
#   src/recomp/recomp_dispatch.h     - binary search dispatch table
#   src/recomp/recomp_types.h        - register model, ICALL macros, MEM helpers

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${GAME_SOURCES} ${RECOMP_GEN_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${GAME_SOURCES} ${RECOMP_GEN_SOURCES})
endif()

# Link against the xboxrecomp umbrella target (all 6 runtime libraries)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

if(WIN32)
    # Windows SDK libraries needed at link time
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        dxgi
        dxguid
        $<IF:$<BOOL:${MINGW}>,xinput1_4,xinput>   # see src/input/CMakeLists.txt
        winmm
        # Lets the fault handler resolve the host RIP back to the generated
        # function's symbol, i.e. to the guest function that faulted.
        dbghelp
    )
else()
    # Generated chunks are huge straight-line functions: -O1 keeps GCC's memory per chunk
    # sane (WSL has ~5 GB); the flags match burnout3-sw's Linux build. The Switch's CPU is
    # the bottleneck there, so it gets -O2 (and no -g: GCC's memory per chunk).
    if(NINTENDO_SWITCH)
        set(ICEAGE2_GEN_OPT "-O2" CACHE STRING "Optimisation for the lifted code")
        set(ICEAGE2_GEN_DEBUG "")
    else()
        set(ICEAGE2_GEN_OPT "-O1" CACHE STRING "Optimisation for the lifted code")
        set(ICEAGE2_GEN_DEBUG "-g")
    endif()
    # recomp_gen_extra.h: the dr*/cr* variables the lifter leaves undeclared.
    set(ICEAGE2_GEN_FLAGS "${ICEAGE2_GEN_OPT};${ICEAGE2_GEN_DEBUG};-w;-fno-strict-aliasing;-fwrapv;-include;${CMAKE_CURRENT_SOURCE_DIR}/src/recomp_gen_extra.h")
    list(REMOVE_ITEM ICEAGE2_GEN_FLAGS "")
    set_source_files_properties(${RECOMP_GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "${ICEAGE2_GEN_FLAGS}")
    if(NOT NINTENDO_SWITCH)
        # -rdynamic: backtrace_symbols names the lifted sub_XXXXXXXX in a crash report.
        target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

if(ICEAGE2_VULKAN)
    # ICEAGE2_VULKAN: main_posix.c installs nv2a_vk; NFSU2_VULKAN: switch_nx.c's name for it.
    target_compile_definitions(${PROJECT_NAME} PRIVATE ICEAGE2_VULKAN=1 NFSU2_VULKAN=1)
endif()

# ── Nintendo Switch homebrew (devkitA64 + libnx) ────────────────
# tools/wsl_build_switch.sh. The NRO reads the extracted disc from sdmc:/switch/iceage2/game/;
# the log and iceage2_env.txt live in sdmc:/switch/iceage2/.
if(NINTENDO_SWITCH)
    set(ICEAGE2_SWITCH_VERSION "0.2.1")
    # No GL: SDL is only the audio output (and joysticks); NVK comes with nv2a_vk.
    target_link_libraries(${PROJECT_NAME} PRIVATE nx m)
    # SDL's audio thread gets a real-time priority and audren gaps are counted (switch_nx.c).
    target_link_options(${PROJECT_NAME} PRIVATE
        "LINKER:--wrap=SDL_SYS_SetThreadPriority"
        "LINKER:--wrap=audrvVoiceAddWaveBuf")
    set_property(SOURCE src/switch_nx.c APPEND PROPERTY
        INCLUDE_DIRECTORIES "${XBOXRECOMP_DIR}/src/apu")
    target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{DEVKITPRO}/portlibs/switch/include")

    set(ICEAGE2_NACP "${CMAKE_CURRENT_BINARY_DIR}/iceage2.nacp")
    nx_generate_nacp(OUTPUT "${ICEAGE2_NACP}"
        NAME "Ice Age 2 (Xbox recomp)"
        AUTHOR "Meluxz"
        VERSION "${ICEAGE2_SWITCH_VERSION}")
    # hbmenu icon: 256x256 JPEG.
    set(ICEAGE2_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.jpg")
    if(EXISTS "${ICEAGE2_ICON}")
        nx_create_nro(${PROJECT_NAME} NACP "${ICEAGE2_NACP}" ICON "${ICEAGE2_ICON}")
    else()
        nx_create_nro(${PROJECT_NAME} NACP "${ICEAGE2_NACP}")
    endif()
endif()

# MSVC-specific: large address aware, increase section limit for big generated files
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
    )
    # The generated code files can be very large. Increase object file section limit.
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    # 45 generated chunks in one target: without /MP MSBuild compiles them one at a time.
    # 3 compilers fit this PC's 12 GB RAM and 4 cores.
    set(ICEAGE2_MP 3 CACHE STRING "cl.exe processes for the generated code (/MP)")
    target_compile_options(${PROJECT_NAME} PRIVATE /MP${ICEAGE2_MP})
endif()

# ── Game-specific additions ─────────────────────────────────────
#
# Add any game-specific libraries, loaders, or custom renderers below.
# Examples:
#
# # Custom texture loader for your game's format
# target_sources(${PROJECT_NAME} PRIVATE src/texture_loader.c)
#
# # If your game uses RenderWare, you may need a RW bridge layer
# target_sources(${PROJECT_NAME} PRIVATE src/rw_bridge.c)
#
# # Additional compile definitions for debugging
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     ENABLE_ICALL_TRACE=1
#     VERBOSE_KERNEL_LOG=1
# )
