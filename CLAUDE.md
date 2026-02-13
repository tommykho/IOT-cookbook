# CLAUDE.md

## Project Overview

IOT-cookbook is a collection of cut-and-paste Arduino C++ (.ino) and MicroPython (.mpy) scripts for microcontroller and IoT projects. It targets ESP32, ESP8266, and ATmega32u4 boards, providing practical solutions for reading sensor data, controlling displays, playing animations, and streaming audio.

Author: Tommy Ho (tommyho510@gmail.com)
License: MIT

## Repository Structure

```
IOT-cookbook/
├── espgfxGIF/               # Main project — animated GIF player for TFT displays
│   ├── espgfxGIF.ino        #   Main sketch (multi-board support)
│   ├── gifdec.cpp / .h      #   GIF decoder library
│   ├── gfx-helper.h         #   Display init, button handling, brightness
│   ├── spiffs-helper.h      #   SPIFFS file system management
│   ├── led-helper.h         #   LED control utilities
│   └── data/                #   GIF files uploaded to device SPIFFS
├── espRadioMP3/             # WiFi radio / MP3 streaming project
│   ├── espRadioMP3.ino      #   Main sketch
│   ├── TFTTerminal.h        #   Text display class for M5StickC
│   └── espRadioTest.cpp     #   Test stub (disabled with #if 0)
├── espgfx_helloworld/       # Minimal hello-world graphics example
├── lilygo-cookbook/          # LILYGO-specific projects
│   └── tdisplay-uncanny-eyes/  # Animated eyes (20+ graphics headers)
├── devices/                 # Per-device documentation, pin configs, reference links
│   ├── lilygo-tdisplay/
│   ├── lilygo-tdisplays3/
│   ├── m5atom-matrix/
│   ├── m5core-basic/
│   ├── m5stickc/
│   └── m5stickc-plus/
├── assets/                  # Image and GIF assets organized by resolution
├── flash_tools/             # ESP32/ESP8266 flashing utilities and binaries
│   └── espROMkit/           #   Python CLI & GUI flash/backup tool (esptool.py)
├── resources/               # Plugins (e.g., arduino-esp32fs-plugin for SPIFFS upload)
└── Readme.md                # Main project README
```

Each project folder is self-contained. Changes in one project do not affect others.

## Build & Development

**IDE**: Arduino IDE 1.8.x or later. There is no Makefile, CMake, or package manager.

**Workflow**:
1. Open the `.ino` sketch in Arduino IDE
2. Select the correct board and COM port in Tools menu
3. Edit the `#define` settings in the configuration section of the sketch
4. Click "Upload" to compile and flash
5. Use "Tools > ESP32 Sketch Data Upload" to upload GIF/data files to SPIFFS

**Key library dependencies** (install via Arduino Library Manager):
- Arduino_GFX Library (v1.2.1+) — graphics rendering
- M5Stack / M5StickC / M5StickC-Plus libraries — board support
- TFT_eSPI — display driver (TTGO/LILYGO devices)
- ESP8266Audio (v1.60) — audio playback (espRadioMP3)
- arduino-esp32fs-plugin — SPIFFS data upload tool

## Code Conventions

### Multi-board support via preprocessor

Board selection uses `#define` and `#if defined()` blocks. Each sketch has a configuration section:

```c
// *** BEGIN editing of your settings ...
#define ARDUINO_M5STICKCPLUS
//#define GIF_FILENAME "/your_file.gif"  /* comment out for random GIF */
//#define DEBUG                           /* uncomment for screen test */
// *** END editing of your settings ...
```

Board-specific code is gated with:
```c
#if defined(ARDUINO_M5STICKCPLUS)
  // M5StickC Plus specific code
#elif defined(ARDUINO_TDISPLAY)
  // T-Display specific code
#endif
```

### Pin definitions

Pin assignments are macros at the top of files or in dedicated headers like `pin_config.h`:
```c
#define TFT_MOSI 15
#define TFT_SCLK 13
#define TFT_DC   23
```

### File headers

Every source file includes a header with version, supported boards, and author:
```c
/*
 *  espgfxGIF Version 2023.09B
 *  Board: T-Display S3, T-Display, M5StickC-Plus, M5StickC
 *  Author: tommyho510@gmail.com
 */
```

### Helper headers

Common functionality is extracted into reusable headers:
- `gfx-helper.h` — display initialization, button handling, brightness control
- `spiffs-helper.h` — SPIFFS mount, file listing, random file selection
- `led-helper.h` — LED on/off control

### Binary data in headers

Large binary assets (logos, images) are stored as `const unsigned char[]` arrays in `.h` files. These files can be very large.

## espROMkit (flash_tools/espROMkit)

Python-based CLI and GUI for backing up and restoring ESP32 flash ROM. Uses [esptool.py](https://github.com/espressif/esptool).

**Run the CLI**: `python flash_tools/espROMkit/espromkit_cli.py`
**Run the GUI**: `python flash_tools/espROMkit/espromkit_gui.py`

**Dependencies**: `pip install -r flash_tools/espROMkit/requirements.txt` (esptool, pyserial). The GUI also requires tkinter (`apt install python3-tk` on Linux).

## Testing

There is no automated test framework. Testing is done manually on physical hardware.

- `Serial.println()` is used for debug output throughout the code
- `#define DEBUG` enables a screen test mode in espgfxGIF
- `espRadioTest.cpp` contains a test stub but is disabled with `#if 0`

## Important Warnings

- **No CI/CD** — there are no GitHub Actions or other automated pipelines. All changes must be verified manually.
- **WiFi credentials** — some sketches contain placeholder WiFi credentials (SSID/password marked with asterisks). Never commit real credentials.
- **SPIFFS size limit** — most ESP32 modules have ~1MB SPIFFS. Keep total data files under 900KB.
- **Binary headers are large** — `.h` files containing image data can be tens of thousands of lines. Avoid reading them in full unless necessary.
- **No dependency lockfile** — library versions are managed manually through Arduino Library Manager. The README documents required versions.
