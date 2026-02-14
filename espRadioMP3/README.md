# espRadioMP3 — WiFi Internet Radio & MP3 Player

Version 2021.01A (MP3 / Voice Edition) | Author: tommyho510@gmail.com
Original Author: Milen Penev

An Arduino sketch that streams internet radio stations and plays MP3 audio on the M5StickC using the ESP8266Audio library. Includes text-to-speech announcements for station names using the Talkie voice synthesizer.

## Features

- **11 preset radio stations** — stream MP3 audio over WiFi from internet radio URLs
- **Button A** — Play / Pause
- **Button B** — Switch station / adjust volume
- **Voice announcements** — Talkie TTS for "Hello", "Ready", "Stop", "Switch", "Pause"
- **Color bar display** — visual test pattern on the M5StickC TFT screen
- **TFT Terminal** — scrolling text display for station info and status

## Tested Dev Modules

| Module | Chip | Notes |
|--------|------|-------|
| M5Stack M5StickC | ESP32-PICO | Primary target, DAC output on GPIO 26 |

## Required Libraries

Install via Arduino Library Manager:

- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) v1.60
- M5StickC library

## File Structure

```
espRadioMP3/
├── espRadioMP3.ino    # Main sketch
├── TFTTerminal.h      # Scrolling text display class for M5StickC LCD
└── espRadioTest.cpp   # Test stub (disabled with #if 0)
```

## Installation

1. Install ESP32 board definitions via Board Manager
2. Install M5StickC library via Library Manager
3. Install ESP8266Audio library (v1.60) via Library Manager
4. Open `espRadioMP3.ino` in Arduino IDE
5. Edit the WiFi credentials and station list:
    ```c
    // *** BEGIN editing of your settings ...
    #define ARDUINO_M5STICKC
    const char *SSID     = "*********";   // Your WiFi SSID
    const char *PASSWORD = "*********";   // Your WiFi password
    // *** END editing of your settings ...
    ```
6. Select M5StickC as the board and the correct COM port
7. Click **Upload**

## Preset Stations

| # | Station | URL |
|---|---------|-----|
| 1 | Mega Shuffle | jenny.torontocast.com:8134 |
| 2 | Orig. Top 40 | ais-edge09-live365-dal02 |
| 3 | Way Up Radio | 188.165.212.154:8478 |
| 4 | Asia Dream | igor.torontocast.com:1025 |
| 5 | KPop Way Radio | streamer.radio.co |
| 6 | Smooth Jazz | sj32.hnux.com |
| 7 | Smooth Lounge | sl32.hnux.com |
| 8 | Classic FM | media-ice.musicradio.com |
| 9 | Lite Favorites | naxos.cdnstream.com |
| 10 | MAXXED Out | 149.56.195.94:8015 |
| 11 | SomaFM Xmas | ice2.somafm.com |

Edit the `arrayURL[]` and `arrayStation[]` arrays to add your own stations.

## Pin Configuration

| Pin | Function |
|-----|----------|
| GPIO 10 | LED |
| GPIO 37 | Button A (Play/Pause) |
| GPIO 39 | Button B (Switch/Volume) |
| GPIO 26 | DAC audio output (Speaker) |

## Notes

- WiFi credentials in the sketch are placeholders (`*********`). Replace with your own before uploading.
- The 32 KB audio buffer can be adjusted via `bufferSize` if you experience stuttering.
- The test file `espRadioTest.cpp` is disabled with `#if 0` and is not compiled.
