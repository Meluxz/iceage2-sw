# Building Ice Age 2 from your own disc

This guide takes you from your own Xbox disc to a Switch `.nro` (and,
optionally, a Linux build to test on PC). It is the same path the port was
made with. Nothing from the game is in this repository: the C code is
generated from **your** `default.xbe`.

Time: about 1 hour of setup, then ~25 min to generate the code and
10-30 min to compile, depending on the machine.

---

## 0. What you need

- **Ice Age 2: The Meltdown**, original Xbox, **NTSC-U** release
  (`default.xbe` title ID `5655004B`, version 1). Other regions or versions
  have different code and will not match `config/` and `src/recomp_manual.c`.
- An image of the disc as an **XISO** (`.iso`), dumped from your own disc:
  for example over FTP from a softmodded Xbox, or with a compatible DVD drive.
- **Linux**, or **Windows with WSL2** (Ubuntu 24.04+). Everything below is
  run in a Linux shell.
- 8 GB of RAM or more (each compiler process takes ~1.5 GB on the generated
  code).
- For the Switch: a console that runs homebrew (Atmosphère), with enough
  space for the game (~1.5 GB).

## 1. Packages

```bash
sudo apt install git cmake ninja-build gcc python3 python3-pip python3-venv \
                 libsdl2-dev libgl1-mesa-dev libepoxy-dev
```

```bash
git clone https://github.com/Meluxz/iceage2-sw.git
cd iceage2-sw
python3 -m venv build/venv
build/venv/bin/pip install capstone
```

## 2. Extract the disc

Extract the XISO into `game/` in the repository (git ignores it):

```bash
python3 tools/xiso.py /path/to/IceAge2.iso extract game
ls game        # default.xbe, IceAge2/, ...
```

Any XISO extractor works (`extract-xiso`, xdvdfs) as long as `game/` ends up
holding `default.xbe` and the `IceAge2` folder.

## 3. Generate the C code

The recompiler disassembles `default.xbe` and translates its ~11,500
functions to C (~1.3 million lines, written to `src/recomp/gen/`, ~70 MB):

```bash
PYTHON=build/venv/bin/python tools/regen.sh
```

It takes 20-30 minutes and ends with `===== done`. You only redo it when the
translator (`xboxrecomp/tools/recomp/`) changes.

## 4. (Optional) Test on PC: Linux build

```bash
JOBS=2 tools/build_linux.sh      # build/linux/iceage2_recomp, ~15-30 min
tools/play.sh                    # log in build/play.log
```

Keyboard: numpad 8/4/2/6 = left stick, arrows = D-pad, Enter = Start,
Backspace = Back, Z/X/A/S = A/B/X/Y, Q/E = White/Black, 1/3 = triggers,
I/J/K/L = right stick. A gamepad works too (SDL).
On WSL the window opens through WSLg; for the GPU instead of software
rendering, `export GALLIUM_DRIVER=d3d12` first.

## 5. Switch build

### 5a. devkitPro

Install devkitPro with the Switch tools (devkitA64, libnx, switch-tools)
following <https://devkitpro.org/wiki/Getting_Started>:

```bash
sudo dkp-pacman -S switch-dev
export DEVKITPRO=/opt/devkitpro
```

(If the package server is not reachable, the `devkitpro/devkita64` Docker
image has the same toolchain in `/opt/devkitpro`.)

### 5b. SDL2 and glslang for the Switch

```bash
tools/build_switch_deps.sh       # SDL2 (switch-sdl-2.28) + glslang 15.4.0 into build/deps
```

### 5c. NVK (the Vulkan driver)

The Switch build renders with **NVK**, Mesa's open-source Vulkan driver,
ported to Horizon by [mesa-switch](https://github.com/danfromtico/mesa-switch)
and linked statically into the NRO. Build it with mesa-switch's own
instructions (its `build-switch.sh` / Docker recipe). What this port needs
from it is the install prefix that holds `lib/libvulkan.a` and
`include/vulkan/`:

```bash
export NVK_SDK=/path/to/mesa-switch/install   # contains lib/libvulkan.a
```

### 5d. Build the NRO

Optional: put a 256x256 JPEG at `assets/icon.jpg` for the hbmenu icon.

```bash
JOBS=2 tools/build_switch.sh     # out/switch/iceage2/iceage2.nro
```

## 6. Install on the Switch

Copy to the SD card:

```
sdmc:/switch/iceage2/iceage2.nro
sdmc:/switch/iceage2/game/        <- everything from your game/ folder
```

Start it from hbmenu with **title takeover**: hold **R** while launching any
installed game, then pick Ice Age 2. Started from the album (applet mode) it
has too little memory.

- The log is `sdmc:/switch/iceage2/iceage2_log.txt`. It is the first thing
  to look at if something goes wrong.
- Options go in `sdmc:/switch/iceage2/iceage2_env.txt`, one `KEY=VALUE` per
  line. For example `RECOMP_NX_PROFILE=1` writes a profile (`prof.bin`) for
  performance work.
- Saves are kept in `sdmc:/switch/iceage2/game/UDATA`.

## Troubleshooting

| Problem | Fix |
|---|---|
| `no capstone` | `PYTHON=build/venv/bin/python tools/regen.sh` (step 1) |
| Compiler killed / out of memory | Lower `JOBS` (1 is safest); on WSL raise its memory in `.wslconfig` |
| `no NVK SDK` | Set `NVK_SDK` to the mesa-switch install (step 5c) |
| Black screen on the Switch | Check `iceage2_log.txt`; make sure `game/default.xbe` is on the SD card |
| Crash when started from the album | Use title takeover (hold R on a game) |
