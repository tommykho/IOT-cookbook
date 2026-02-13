# IOT-cookbook — Arduino & MicroPython Scripts for Microcontrollers

A collection of cut-and-paste Arduino C++ and MicroPython scripts for ESP32, ESP8266, and ATmega32u4 microcontroller projects.

**Build practical solutions to read sensor data, control displays, play animations, stream audio, and flash firmware using popular dev boards.**

Author: Tommy Ho (tommyho510@gmail.com)
License: MIT

## Projects

| Project | Description | Board(s) |
|---------|-------------|----------|
| [espgfxGIF](espgfxGIF/) | Animated GIF player for TFT displays | M5StickC-Plus, M5StickC, T-Display S3, T-Display, D1 Mini |
| [espRadioMP3](espRadioMP3/) | WiFi internet radio / MP3 streaming player | M5StickC |
| [espgfx_helloworld](espgfx_helloworld/) | Minimal hello-world graphics example | M5StickC-Plus, M5StickC, T-Display S3, T-Display, D1 Mini |
| [tdisplay-uncanny-eyes](lilygo-cookbook/tdisplay-uncanny-eyes/) | Animated eyes with 30 swappable eye styles | TTGO T-Display |
| [espROMkit](flash_tools/espROMkit/) | CLI & GUI tool to back up and restore ESP32 flash ROM | Any ESP32 board |

## Supported Microcontrollers

**ESP32:**
- M5Stack M5StickC / M5StickC-Plus (ESP32-PICO-D4)
- M5Stack Basic Core / M5 Faces (ESP32)
- M5Stack ATOM Matrix (ESP32)
- LilyGO TTGO T-Display (ESP32 + ST7789)
- LilyGO T-Display S3 (ESP32-S3 + ST7789)
- TTGO T-Camera w/ OLED, TTGO T-Journal w/ OLED
- Ai-Thinker ESP32-CAM / ESP-32S
- HELTEC HTIT-WB32 w/ OLED (ESP32)
- NodeMCU ESP32

**ESP8266:**
- NodeMCU V3
- WeMos D1 Mini
- WeMos WiFi-ESP8266

**Other:**
- Adafruit Circuit Playground Classic / Express (ATmega32u4)
- M5Stack UnitV (Kendryte K210)

## Repository Structure

```
IOT-cookbook/
├── espgfxGIF/                  # Animated GIF player (main project)
├── espRadioMP3/                # WiFi radio / MP3 streaming
├── espgfx_helloworld/          # Hello-world graphics example
├── lilygo-cookbook/
│   └── tdisplay-uncanny-eyes/  # Animated eyes for T-Display
├── flash_tools/
│   └── espROMkit/              # ESP32 flash backup/restore tool (Python)
├── devices/                    # Per-device pin configs and documentation
├── assets/                     # GIF and image assets by resolution
└── resources/                  # Arduino IDE plugins (esp32fs)
```

## Quick Start

1. Install [Arduino IDE](https://www.arduino.cc/en/software) 1.8.x or later
2. Install board definitions for ESP32 and ESP8266 via Board Manager
3. Install required libraries via Library Manager (see each project's README)
4. Open the `.ino` sketch, select your board and COM port
5. Edit the `#define` in the `// *** BEGIN ... END ***` settings block for your board
6. Click **Upload** to compile and flash

## Languages & Tools

- [Arduino](https://www.arduino.cc/) C++ (.ino) — all microcontroller sketches
- [Python](https://www.python.org/) — espROMkit flash tool (uses [esptool.py](https://github.com/espressif/esptool))
- [MicroPython](https://micropython.org/) (.mpy) / [M5Flow](http://flow.m5stack.com/) (.m5f) — referenced for future projects

## About the Author

Tommy Ho has been coding since before the web existed, with a passion for Python, R, and Arduino for over a decade. This cookbook grew from his work on automation, microcontroller, and robotics projects.

## Suggestions and Feedback

[Click here](mailto:tommyho510@gmail.com) for feedback or suggestions.
