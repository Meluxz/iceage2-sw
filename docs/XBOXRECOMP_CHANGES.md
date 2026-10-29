# What this port changed in xboxrecomp

`xboxrecomp/` is the toolkit as vendored in
[burnout3-sw](https://github.com/antoxa2584x/burnout3-sw) (commit `28663a6`,
v0.2.4), itself based on [xboxrecomp](https://github.com/sp00nznet/xboxrecomp).
These are the changes made for Ice Age 2. The translator fixes are general:
any title compiled the same way hits the same bugs.

## Translator (`tools/recomp/`)

| Change | Symptom in Ice Age 2 |
|---|---|
| `lahf` after an x87 `fcomi`/`fucomip` now builds AH from the compare. It was left as a comment, so MSVC's float `==` idiom (`fucomip; fstp st0; lahf; test ah, 44h; jnp`) tested whatever AH held. | Scrat's animation state machine froze on ice and ledges (59 sites). |
| One-operand `mul`/`imul`/`div`/`idiv` on 8- and 16-bit operands use AX / DX:AX. They were lifted as the 32-bit forms, so `imul dl` multiplied all of EAX and overwrote EDX. | The underwater ripple grid read its row from EDX: three corners of every quad went to row 0 and the screen filled with streaks (16 sites). |
| `fprem` sets the compare state it leaves, and unresolved stubs can log the first time they run (`RECOMP_STUB_LOG`). | Debugging aid; a partial-remainder loop that never ended. |

## Renderers (`src/nv2a_gl/`, `src/nv2a_vk/`)

| Change | Symptom |
|---|---|
| Clears of R5G6B5 / X1R5G5B5 surfaces unpack the packed clear value. | Snow tinted blue (the shadow map was cleared to "565 white" read as ARGB). |
| `SET_TEXTURE_BORDER_COLOR` applied to clamp-to-border sampling. | Distant ground black. |
| W-buffered depth (CONTROL0 Z_PERSPECTIVE_ENABLE): depth from clip w. | Menu acorns drawn black, behind the ice frame. |
| GL: persistent-mapped vertex/index rings (GL 4.4). | 12-16 fps -> 28-29 fps in the first level on PC. |
| Probe tooling: `RECOMP_GL_PROBE` (per-pixel draw history, depth, texgen, vertices), `RECOMP_GL_DUMP` frame ranges. | How the above were found. |

## Kernel and runtime (`src/kernel/`, `src/input/`, `src/usb/`)

| Change | Symptom |
|---|---|
| `RtlEnterCriticalSection` / `RtlLeaveCriticalSection` keep the guest lock (Enter lets go only to block). | A third of the Switch main thread went to lock handovers: 2.25M of each call. |
| `RECOMP_FLIP_STALL=0`, `RECOMP_VBLANK_HZ`, pushbuffer wait timing (`RECOMP_PB_WAITS`). | Frame pacing and diagnosis. |
| Executor: `RECOMP_INLINE_DUMP` (inline vertex arrays), larger pad scripts. | Diagnosis. |
| POSIX keyboard stubs on the Switch build. | Build fix. |

## Defaults set by this port (`src/main_posix.c`)

- `RECOMP_GIL_EAGER=1`: the time-critical music streamer gets the guest lock
  first (stale ring audio 3-10% -> under 1%, no crackle).
- Switch: `RECOMP_CULL=0`: the executor's CPU off-screen test cost 22% of the
  render thread for 22% of the draws skipped.
