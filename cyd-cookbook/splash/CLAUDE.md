# CLAUDE.md

Instructions for Claude Code working in this repository.

## What this is

`espgfxSplash` — an animated GIF boot splash for ESP32 TFT boards. Plays a
GIF N times with a software fade-in/fade-out, scales it to whatever panel is
attached, then hands off to a main menu.

Successor to `espgfxGIF` 2022.05B in
[IOT-cookbook](https://github.com/tommykho/IOT-cookbook). The key design
change: the original faded by dimming the backlight, which requires PWM
control the newer target boards do not all have. This version multiplies
each pixel's R/G/B by a 0–255 fade factor on the way to the panel instead.

## Current state

**Nothing compiles yet.** The blocking item is the gifdec patch.

- [x] Sketch written, six board configs
- [x] `gif_source.h` abstraction written
- [x] Test pattern GIF and `logo_gif.h` generated
- [x] Wokwi project assembled and compiling in-browser
- [ ] **`gifdec.cpp` patched — `gd_open_gif_mem()` does not exist yet**
- [ ] Verified on any physical board
- [ ] Pin maps confirmed for anything except the 2432S028R

### The gifdec patch

`gifdec` reads from a `File`. It needs to read from a flash array too. It
only ever does two things — read N bytes, seek to an offset — so the change
is small. Per the header comment in `gif_source.h`:

1. `#include "gif_source.h"` in `gifdec.h`
2. Replace the `File`/`fd` member of `gd_GIF` with `gd_src src;`
3. Every read becomes `gd_src_read(&gif->src, buf, n)`; every seek becomes
   `gd_src_seek(&gif->src, pos)`
4. Add `gd_open_gif_mem(const uint8_t *data, size_t len)` — the existing
   `gd_open_gif` with `gd_src_from_mem()` in place of the file open

Do not fork gifdec into two versions. One decoder, two sources.

## Build and test

```sh
pio run                              # compile
pio run -t upload                    # flash
pio device monitor -b 115200         # serial

wokwi-cli .                          # headless sim (needs WOKWI_CLI_TOKEN)
wokwi-cli . --scenario <file>.yaml   # automated check
```

Wokwi runs the simulation in Wokwi's cloud — the firmware binary leaves the
machine. Fine for this project; worth remembering before pointing it at
anything proprietary.

Physical board on USB is the primary debug path. Wokwi is a public demo
shopfront, not the dev loop.

## Hard constraints

These are not preferences. Violating them produces builds that fail at
runtime rather than at compile time.

**No full framebuffer on the WROOM boards.** 320×480 × 2 = 307 KB against
~300 KB usable DRAM. The row-by-row streaming in `pushFrame()` exists for
this reason. Do not "simplify" it into a canvas. The JC3248W535C is the only
target with the PSRAM to afford one, and it already has `NEEDS_CANVAS_FLUSH`.

**Arduino_GFX must be ≥ 1.4.3.** `Arduino_AXS15231B` and
`Arduino_ESP32QSPI` do not exist before that. The original sketch targeted
1.2.1; assume nothing carries over.

**Wokwi cannot upload a LittleFS image.** Anything intended to run in the
simulator needs `GIF_FROM_FLASH 1`.

**`Arduino_ESP32LCD8` was removed in arduino-esp32 v3.0.** Affects the
T-Display S3 block only. Pin the core to 2.x or move to
`Arduino_ESP32PAR8`.

**Never invent pin numbers.** Cross-check against Arduino_GFX's
`Arduino_GFX_dev_device.h`, which ships with the library and tracks
hardware revisions. Backlight in particular varies within the CYD family
(21 on the 2.8", 27 on the 3.5").

**`DEMO_REPLAY_MS` must be 0 in shipped firmware.** It is a demo affordance
for the public Wokwi link.

## Conventions

- Ask clarifying questions in rounds until ~95% confident before executing.
  Guessing wrong on embedded work costs a reflash cycle. See the `/95p`
  skill.
- Run the tool, show the result, stop. No preamble.
- Comments explain *why*, not *what*. Every constraint above earned its
  comment by being non-obvious.
- Board configs live in one `#if defined(BOARD_*)` chain in the sketch. Do
  not scatter them across headers.
- Keep the sketch single-target-agnostic: everything derives from
  `gfx->width()` / `gfx->height()`, never a hardcoded resolution.

## Debugging aids already in place

- Serial prints GIF dimensions, computed destination rect, and frame buffer
  size on every splash. `120x90 -> 240x180 at (0,70)` is correct for a
  120×90 source on a 240×320 panel.
- On allocation failure it prints the largest free heap block, so you can
  tell "GIF too big" from "heap fragmented".
- The 2432S028R block defines the onboard RGB LED (pins 4/16/17, active
  LOW). Useful because a fade renders a legitimately black screen that looks
  identical to a crash.
- `logo.gif` is a diagnostic pattern, not artwork — border catches scaling
  errors, RGB swatches catch channel swaps, progress bar catches frame
  ordering. See README for the full mapping.

## Things that have already been ruled out

Do not re-propose these; each was investigated and rejected for a reason.

- **Arduino Cloud Editor** — no filesystem upload, no board definitions for
  these panels, still needs a local agent to flash.
- **Backlight PWM fading** — not available on all targets; that limitation
  is why this project exists.
- **Pre-rendered RGB565 frame arrays** — 38 KB per frame at 160×120 versus
  ~3.5 KB for the whole compressed GIF.
- **A separate Wokwi-specific sketch** — was tried, then merged back. One
  sketch, switched by `BOARD_*` and `GIF_FROM_FLASH`.
