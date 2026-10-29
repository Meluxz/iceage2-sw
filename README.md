<div align="center">

## Ice Age 2: The Meltdown — Switch / Linux

### Xbox static recompilation for Nintendo Switch and Linux

The original Xbox (NTSC-U) release of **Ice Age 2: The Meltdown**, lifted
instruction by instruction to C and running natively, with no emulator.

![Switch](https://img.shields.io/badge/Nintendo%20Switch-homebrew-E60012?logo=nintendoswitch&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-x86__64-FCC624?logo=linux&logoColor=black)
![Vulkan](https://img.shields.io/badge/Vulkan-NVK-AC162C?logo=vulkan&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-renderer-5586A4?logo=opengl&logoColor=white)
![Version](https://img.shields.io/badge/version-0.2.1-blue)

</div>

---

> [!IMPORTANT]
> **No game data is included**, and none of the code generated from it.
> You need your own copy of the Xbox disc; the C code is generated from your
> own `default.xbe`. **[How to build it from your disc → docs/BUILDING.md](docs/BUILDING.md)**

## ✨ Features

- 🦫 **Native code**: the whole game runs as recompiled C (~11,500 functions),
  built with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
- 🎮 **Two renderers**: Vulkan (NVK on Switch) and OpenGL (Linux), translating
  the NV2A pushbuffer, vertex programs and register combiners at run time
- 🔊 **Audio** through an emulated Xbox APU, out through SDL2
- 🎬 **Movies** (XMV) play
- 🕹️ Joy-Cons / Pro Controller (libnx); keyboard or gamepad on Linux
- ⏱️ **30 fps**, as on the Xbox

## 🕹️ Playing on Switch

1. Copy `iceage2.nro` to `sdmc:/switch/iceage2/`.
2. Copy the **extracted** disc (not the ISO) to `sdmc:/switch/iceage2/game/`.
3. Start it with **title takeover**: hold **R** while launching any game.
   Applet mode leaves too little memory.

The log is written to `sdmc:/switch/iceage2/iceage2_log.txt`; options go in
`sdmc:/switch/iceage2/iceage2_env.txt` (`KEY=VALUE` per line).

## 🛠️ Building

Linux or WSL2. Full walkthrough, from your disc to the NRO:
**[docs/BUILDING.md](docs/BUILDING.md)**.

<details open>
<summary><b>0. Extract your disc</b></summary>

```bash
# Writes game/ (default.xbe, IceAge2/, ...) -- git ignores it
python3 tools/xiso.py /path/to/IceAge2.iso extract game
```

</details>

<details open>
<summary><b>1. Lift the XBE to C</b></summary>

```bash
# Needs Python 3 with capstone (pip install capstone). Writes src/recomp/gen (~70 MB, ~25 min)
XBE=/path/to/game/default.xbe PYTHON=python3 tools/regen.sh
```

</details>

<details open>
<summary><b>2a. Linux</b> (SDL2 + OpenGL)</summary>

```bash
tools/build_linux.sh                                   # build/linux/iceage2_recomp
ICEAGE2_GAME_DIR=/path/to/game tools/play.sh
```

</details>

<details open>
<summary><b>2b. Nintendo Switch</b> (devkitA64, libnx, NVK from mesa-switch, glslang)</summary>

```bash
tools/build_switch_deps.sh                             # SDL2 + glslang for the Switch, into build/deps
# Vulkan build (needs mesa-switch NVK); optional hbmenu icon: assets/icon.jpg (256x256)
NVK_SDK=/path/to/nvk JOBS=2 tools/build_switch.sh      # out/switch/iceage2/iceage2.nro
```

</details>

## 📁 Layout

| Path | What |
|---|---|
| `src/` | Boot, Switch layer, hand-written fixes (`recomp_manual.c`) |
| `xboxrecomp/` | The recompilation toolkit and runtime (kernel, NV2A, APU, input) |
| `config/` | Extra function entry points for the translator |
| `tools/` | Disc extraction, code generation and build scripts |
| `docs/` | Build guide |

## 📊 Status

- Menus, movies and the first levels are playable
- 30 fps in menus and levels on Switch
- Saves load and save (`game/UDATA`)

See [CHANGELOG.md](CHANGELOG.md).

## 🙏 Credits

- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp): the static recompiler (MIT)
- [burnout3-sw](https://github.com/antoxa2584x/burnout3-sw): the Switch runtime this builds on (MIT)
- [xemu](https://xemu.app): NV2A / APU reference and parts of the audio code (LGPL-2.1)
- [mesa-switch](https://github.com/danfromtico/mesa-switch) (NVK), glslang, SDL2, devkitPro / libnx

Port by **Meluxz**.

## 📜 License

MIT ([LICENSE](LICENSE)). `xboxrecomp/` keeps its own licence and notices
(MIT, with LGPL-2.1 parts from xemu: see `xboxrecomp/NOTICE`).
Ice Age 2 is © its respective owners; this project contains none of its data.
