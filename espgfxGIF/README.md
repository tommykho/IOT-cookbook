# espgfxGIF — Arduino Animated GIF Player

Version 2023.09B | Author: tommyho510@gmail.com
Project page: https://www.hackster.io/projects/8964df

An Arduino sketch that plays animated GIFs on TFT screens of ESP32 and ESP8266 dev modules using [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) and [gifdec](https://github.com/lecram/gifdec).

![Demo](https://hackster.imgix.net/uploads/attachments/1245416/espgfxgif_Ew48AHPOEB.gif?auto=format%2Ccompress&gifq=35&w=900&h=675&fit=min&fm=mp4)

## Features

- **Button A** — Invert display
- **Button B** — Adjust brightness
- **Power button** — Reboot and play the designated animated GIF, or a random GIF from SPIFFS if `GIF_FILENAME` is not defined
- **High performance** — up to 70 fps for a 135x240 animated GIF on ESP32

![Demo](https://hackster.imgix.net/uploads/attachments/1440216/reboot_gif_rpzYJ4hATJ.gif?auto=format%2Ccompress&gifq=35&w=740&h=555&fit=max&fm=mp4)
![Demo](https://hackster.imgix.net/uploads/attachments/1633657/ezgif-2-dfa94361f4_YnwKZ7CTPy.gif?auto=format%2Ccompress&gifq=35&w=740&h=555&fit=max&fm=mp4)

## Tested Dev Modules

| Module | Chip | Display |
|--------|------|---------|
| M5Stack M5StickC-Plus | ESP32-PICO | ST7789 135x240 |
| M5Stack M5StickC | ESP32-PICO | ST7735 80x160 |
| LilyGO T-Display S3 | ESP32-S3 | ST7789 170x320 |
| LilyGO T-Display | ESP32 | ST7789 135x240 |
| Wokwi ESP32 Emulator | ESP32 | ILI9341 |
| D1 Mini + TFT-2.4 shield | ESP8266 | ILI9341 240x320 |

## Required Libraries

Install via Arduino Library Manager:

- [Arduino_GFX Library](https://github.com/moononournation/Arduino_GFX) v1.2.1+
- M5StickC / M5StickC-Plus library (for M5Stack boards)
- [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) — for uploading GIF files to SPIFFS

## File Structure

```
espgfxGIF/
├── espgfxGIF.ino      # Main sketch
├── gifdec.cpp         # GIF decoder
├── gifdec.h           # GIF decoder header
├── gfx-helper.h       # Display init, button handling, brightness
├── spiffs-helper.h    # SPIFFS mount, file listing, random selection
├── led-helper.h       # LED control
└── data/              # GIF files (uploaded to SPIFFS)
    ├── homervanish_240x135.gif
    ├── somsomi0309ff_240x135.gif
    └── suatmm_240x135.gif
```

## Installation

**Setup time: ~15 minutes. GIF upload: ~1 minute. Compile and flash: ~2 minutes.**

1. Download [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) and unzip into `%USERPROFILE%\Documents\Arduino\tools` (Windows) or `~/Arduino/tools` (macOS/Linux)
2. Restart Arduino IDE 1.8.x
3. Install ESP32 and ESP8266 board definitions via Board Manager
4. Install M5StickC / M5StickC-Plus library via Library Manager
5. Install "GFX Library for Arduino" (Arduino_GFX) via Library Manager
6. Clone or download this repository
7. Place your animated GIF files in the `espgfxGIF/data/` folder (keep total under 900 KB for 1 MB SPIFFS)
8. Open `espgfxGIF.ino` in Arduino IDE
9. Connect your dev module and select the correct board + COM port
10. Edit the settings block at the top of the sketch:
    ```c
    // *** BEGIN editing of your settings ...
    #define ARDUINO_M5STICKCPLUS
    //#define GIF_FILENAME "/suatmm_240x135.gif" /* comment out for random GIF */
    //#define DEBUG /* uncomment this line to start with screen test */
    // *** END editing of your settings ...
    ```
11. Click **Upload** to compile and flash the sketch
12. Select **Tools > ESP32 Sketch Data Upload** to upload GIF files to SPIFFS

## Adding a New Board

If you're not using M5StickC or TTGO T-Display, add your own configuration block after the existing `#elif` sections. You need to declare your canvas/data-bus class, pin definitions (MOSI, SCLK, CS, DC, RST, BL), and button pins.

References:
- [Arduino_GFX Canvas Class](https://github.com/moononournation/Arduino_GFX/wiki/Canvas-Class)
- [Arduino_GFX Data Bus Class](https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class)

Example for Raspberry Pi Pico with GC9A01:
```c
#elif defined(ARDUINO_RPI_PICO)
Arduino_DataBus *bus = new Arduino_RPiPicoSPI(27, 17, PIN_SPI0_SCK, PIN_SPI0_MOSI, PIN_SPI0_MISO, spi0);
Arduino_G *output_display = new Arduino_GC9A01(bus, TFT_RST, 0, true);
Arduino_GFX *gfx = new Arduino_Canvas(240, 240, output_display);
```

## Background

TFT_eSPI supports BMP and MJPEG/JPEG but not animated GIF due to how GIF handles color palettes. Adafruit_GFX also lacks animated GIF support. Arduino_GFX (a rewrite of Adafruit_GFX and TFT_eSPI) supports various displays with various data bus interfaces. Combined with gifdec for frame decoding, animated GIFs can be played on TFT displays without color corruption.

![GIF Demo](https://hackster.imgix.net/uploads/attachments/1245442/suatmm_240x135_zJFUCGVJwh.gif?auto=format%2Ccompress&gifq=35&w=740&h=555&fit=max&fm=mp4)
![GIF Demo](https://hackster.imgix.net/uploads/attachments/1245443/ezgif_com-gif-maker_lT0n9V0Nyu.gif?auto=format%2Ccompress&gifq=35&w=740&h=555&fit=max&fm=mp4)

## Known Bugs

- Only plays a single GIF file in a loop until power cycle
- Minor artifact with speaker
- Minor artifact with M5StickC

## Features in Progress

- Multiple devices sync (playing GIF/MPEG in synchronization)
- Gesture control with IMU
- Play multiple GIF files in a loop
- MP3 or Radio playback (see [espRadioMP3](../espRadioMP3/))
- BLE connection for wireless GIF upload
- IFTTT and MQTT integration for remote power cycle or GIF change
- MJPEG support (live webcam or IP CAM)

## Donations

Feel free to buy me a coffee: https://www.paypal.com/donate/?business=YF44M264KV8CC&no_recurring=1&currency_code=USD
