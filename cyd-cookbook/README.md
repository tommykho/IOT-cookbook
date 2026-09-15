# cyd-cookbook

Projects, firmware backups and notes for "Cheap Yellow Display" style ESP32 touch boards.

| Folder | What |
|---|---|
| [cyd-video-player](cyd-video-player/) | MJPEG video player from SD card (ESP32-3248S035, ESP32-2432S028) |
| [splash](splash/) | Animated GIF boot splash with fade in/out |
| [devices/ESP32-3248S035](devices/ESP32-3248S035/) | Factory firmware backup, `videoplayer.bin`, `restore.cmd` |
| [devices/JC3248W535C](devices/JC3248W535C/) | Factory firmware backup (Guition 3.5" ESP32-S3), `restore.cmd` |
| `pic/` | Sample JPEGs |
| `music/` | SD card layout for MP3 players (`wifi.json` placeholder; MP3s not in git) |
| `mjpeg/` | SD card videos (not in git; convert with the ffmpeg command in cyd-video-player) |

## Boards

| Board | MCU | Flash / PSRAM | Display | Touch |
|---|---|---|---|---|
| ESP32-3248S035 | ESP32-D0WD-V3 | 4MB / none | 3.5" 320x480 ST7796, SPI | Resistive XPT2046 |
| JC3248W535C | ESP32-S3 | 16MB / 8MB | 3.5" 320x480 AXS15231B, QSPI | Capacitive |
| ESP32-2432S028R | ESP32 | 4MB / none | 2.8" 240x320 ILI9341, SPI | Resistive XPT2046 |

ESP32-3248S035 pins: display DC 2, CS 15, SCK 14, MOSI 13, MISO 12, backlight 27; touch CS 33, IRQ 36 (shares display SPI); SD CS 5, MOSI 23, SCK 18, MISO 19; RGB LED 4/16/17; LDR 34; speaker 26.

## Firmware backups

Full-flash dumps made with `esptool read-flash 0 ALL`, each with a `.sha256`. Backups contain no user WiFi credentials (the JC3248W535C NVS holds only its factory AP defaults).

Restore / flash (write at offset 0):
```
python -m esptool --port COMx --before no-reset --baud 460800 write-flash 0 <file>.bin
```
- **ESP32-3248S035**: auto-download does not work, hold BOOT, tap RST, release BOOT before flashing. `videoplayer.bin` is the cyd-video-player build (merged image).
- **JC3248W535C**: native USB (USB-Serial/JTAG), no button needed.

## Video performance (measured, MJPEG from SD)

| Board | Video | FPS |
|---|---|---|
| ESP32-3248S035 | 480x320 | 11 (decode 52 ms + SPI draw 38 ms) |
| ESP32-3248S035 | 320x214 | 24.5 (decode 23 ms + draw 17 ms) |

Classic ESP32 boards without PSRAM on single-line SPI top out around 11 fps full-screen at 320x480. For video at this resolution use an ESP32-S3 board with PSRAM and a QSPI/parallel/RGB display (JC3248W535C, T-Display S3).

SD cards must be FAT32 (exFAT is not supported by the ESP32 Arduino core); on large cards create a ≤32GB FAT32 partition.
