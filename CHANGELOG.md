# Changelog

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
