# Changelog

## 0.2.2

- Sid's sled mini-game: no more crash, and Sid rides the sled instead of
  floating above it. Six game functions reached only through vtables were
  never found by the disassembler and so never translated (0x000DF9F0,
  0x000AE310, 0x000AE670, 0x000E8090, 0x00243730, 0x0025DB0C); they are now
  seeds in config/seed_functions.json, found with a scan of the XBE's data
  for code pointers that are not known functions

## 0.2.1

- Switch: hbmenu icon, author and version
- Switch: 30 fps in levels: the off-screen culling test (22% of the render
  thread) is off, and Rtl critical sections keep the guest lock
- Audio: no more crackle -- the music streamer gets the guest lock by priority
- Underwater view: no more streaks (one-operand 8/16-bit mul/imul/div/idiv
  were translated as 32-bit)
- Scrat's animations no longer freeze on ice and ledges (x87 `lahf` after
  `fucomip` was not translated)
- Menu acorns no longer black (W-buffer depth)

## 0.1.0

- First Switch NRO (Vulkan / NVK)
- Correct textures: R5G6B5 clears, texture border colour
- Linux build at 28-29 fps in the first level
