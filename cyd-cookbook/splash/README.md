# espgfxSplash

Animated GIF boot splash for ESP32 TFT boards, with a software fade-in and
fade-out, automatic scaling, and a clean handoff to your main menu.

Successor to `espgfxGIF` 2022.05B (Brightness Edition) from
[IOT-cookbook](https://github.com/tommykho/IOT-cookbook). The original
dimmed the backlight, which only works on boards that expose backlight PWM.
This version scales the pixels instead, so the fade works everywhere —
including panels with no brightness control at all.

---

## Supported devices

| Board | MCU | Panel | Resolution | Bus | PSRAM | Status |
|---|---|---|---|---|---|---|
| **ESP32-2432S028R** (CYD 2.8") | ESP32-WROOM-32 | ILI9341 | 240×320 | HSPI | — | Pin map confirmed |
| **ESP32-3248S035** (CYD 3.5") | ESP32-WROOM-32 | ST7796 | 320×480 | HSPI | — | Pins unverified |
| **ESP32-2432S032** (CYD 3.2") | ESP32-WROOM-32 | ST7789 | 240×320 | HSPI | — | Pins unverified |
| **JC3248W535C** (Guition 3.5") | ESP32-S3-WROOM-1 | AXS15231B | 320×480 | QSPI | 8 MB | Needs canvas + flush |
| **LilyGO T-Display S3** | ESP32-S3 | ST7789 | 170×320 | 8-bit parallel | 8 MB | Core 2.x only |
| **M5StickC Plus** | ESP32-PICO | ST7789 | 135×240 | VSPI | — | Pins unverified |

**Nothing here has been validated on physical hardware yet.** Only the
2432S028R pin map is independently confirmed, because Wokwi ships a board
part for it. Every other pin map mirrors Arduino_GFX's
`Arduino_GFX_dev_device.h` — if a board shows a white or garbled screen,
check that header first; it is the authoritative source and tracks the
library.

### Per-board notes

**JC3248W535C** — direct rendering to this panel is unreliable; it needs an
`Arduino_Canvas` framebuffer with an explicit `flush()`. With 8 MB PSRAM a
full 320×480 canvas is affordable here, which it is not on the CYDs. The
`NEEDS_CANVAS_FLUSH` define handles this.

**LilyGO T-Display S3** — uses `Arduino_ESP32LCD8`, which Espressif removed
in arduino-esp32 v3.0. Either pin the core to 2.x or swap the bus to
`Arduino_ESP32PAR8`.

**Backlight pins differ across the CYD family** — 21 on the 2.8", 27 on the
3.5". Do not copy that line between board blocks.

---

## Memory

The no-PSRAM boards cannot hold a full framebuffer. 320×480 × 2 bytes is
307 KB against roughly 300 KB of usable DRAM. This sketch never allocates
one. It scales and fades row by row, streaming straight to the panel, so
runtime cost is only:

```
gifFrameBuf = gifW × gifH × 3    (RGB888, gifdec's output)
lineBuf     = dstW × 2           (one destination row, RGB565)
```

Source GIF size is therefore the real constraint, not screen size:

| GIF | Frame buffer | Verdict on 4 MB / no PSRAM |
|---|---|---|
| 120×90 | 32 KB | comfortable |
| 160×120 | 58 KB | fine |
| 240×135 | 97 KB | near the ceiling |
| 320×240 | 230 KB | will not fit |

Scaling is nearest-neighbour in 16.16 fixed point, so a small source
stretched to a large panel stays cheap.

---

## Requirements

- **Arduino_GFX ≥ 1.4.3.** Not 1.2.1. The `AXS15231B` driver and
  `Arduino_ESP32QSPI` bus do not exist in the old version.
- **gifdec** (`gifdec.h` / `gifdec.cpp`), patched per `gif_source.h`.
- Arduino IDE 2.x or PlatformIO. Arduino Cloud Editor will not work — it has
  no filesystem upload and no board definitions for these panels.

---

## Where the GIF lives

Set `GIF_FROM_FLASH` at the top of the sketch.

**`1` — compiled into flash** from `logo_gif.h`. Recommended, and required
for Wokwi and for boards with no SD slot. A splash is a fixed brand asset;
shipping it with the firmware means it can never be stale, missing, or
unflashed.

```sh
xxd -i logo.gif > logo_gif.h
# then rename the symbols to logo_gif / logo_gif_len
```

**`0` — read from LittleFS** at `GIF_FILENAME`. Use when the logo should be
swappable without recompiling (white-label builds, customer branding).

Uploading to LittleFS from Arduino IDE 2.x needs the
[arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload)
extension — the old ESP32FS plugin is 1.8.x only. Note that the extension
broke on arduino-esp32 core 3.0.7+, which moved `esptool.py`; pin the core to
2.x, use a newer plugin release, or build the image manually with
`mklittlefs`.

---

## Configuration

All at the top of `espgfxSplash.ino`.

| Define | Default | Purpose |
|---|---|---|
| `BOARD_*` | `ESP32_2432S028R` | Target board — uncomment exactly one |
| `GIF_FROM_FLASH` | `1` | 1 = flash array, 0 = LittleFS |
| `GIF_FILENAME` | `/logo.gif` | LittleFS path, ignored when flash-backed |
| `SPLASH_LOOPS` | `1` | Times to play before fading out |
| `FADE_IN_MS` | `600` | Ramp from black at startup |
| `FADE_OUT_MS` | `600` | Ramp to black after the last loop |
| `SCALE_TO_FIT` | `1` | 1 = scale and centre, 0 = centre at 1:1 |
| `DEMO_REPLAY_MS` | `4000` | Replay every N ms; **set to 0 for real firmware** |
| `DEBUG` | on | Serial diagnostics |

---

## Live demo

[Wokwi simulation](https://wokwi.com/projects/475157903687968769) — runs the
2432S028R board part in-browser, no hardware needed.

Wokwi cannot upload a LittleFS image, so the demo requires
`GIF_FROM_FLASH 1`. `DEMO_REPLAY_MS` should be non-zero there so a visitor
arriving mid-run sees the animation rather than a static menu screen.

---

## Test pattern

`logo.gif` is a generated diagnostic, not artwork. Each element fails in a
visually distinct way:

| Element | Catches |
|---|---|
| 1px white border | Scaling or centring off by even one pixel |
| Corner ticks | A corner falling outside the panel |
| Centre crosshair | Centring maths |
| R/G/B swatches | RGB565 channel order; uneven fade across channels |
| Orbiting dot | Frames not advancing |
| Progress bar | Frame order, so a bad rewind is obvious |

Regenerate or resize with `make_test_gif.py`.

---

## Files

```
espgfxSplash.ino    main sketch, all board configs
gif_source.h        file/flash abstraction for gifdec
logo_gif.h          generated GIF blob
make_test_gif.py    regenerates logo.gif and logo_gif.h
diagram.json        Wokwi circuit
wokwi.toml          Wokwi VS Code config
platformio.ini      PlatformIO build config
gifdec.h/.cpp       GIF decoder (from IOT-cookbook, patched)
```

---

## Credits

GIF decoder and the original `espgfxGIF` player by
[moononournation](https://github.com/moononournation/Arduino_GFX).
Splash, fade, and scaling by Tommy Ho / AutonomousQ.
