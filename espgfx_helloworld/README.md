# espgfx_helloworld — Minimal Graphics Example

Version 2023.09A (Basic Edition) | Author: tommyho510@gmail.com
Original Author: moononournation

A minimal Arduino sketch that initializes a TFT display using [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) and prints "Hello World!" with the screen resolution and rotation. Useful as a starting point for new display projects or to verify your board configuration.

## What It Does

1. Initializes the TFT display via Arduino_GFX
2. Prints "Hello World!" along with the screen resolution and rotation number
3. Waits 3 seconds, then enters a loop that flashes the LED and inverts the display every second

## Supported Boards

| Board Define | Module | Display |
|-------------|--------|---------|
| `ARDUINO_M5STICKCPLUS` | M5StickC-Plus | ST7789 135x240 |
| `ARDUINO_M5STICKC` | M5StickC | ST7735 80x160 |
| `ARDUINO_TDISPLAYS3` | T-Display S3 | ST7789 170x320 |
| `ARDUINO_TDISPLAY` | TTGO T-Display | ST7789 135x240 |
| `ARDUINO_D1MINI` | D1 Mini + TFT-2.4 | ILI9341 240x320 |
| `ARDUINO_DEVKITV1` | ESP32 DevKit V1 + ILI9341 | ILI9341 240x320 |

## Required Libraries

Install via Arduino Library Manager:

- [Arduino_GFX Library](https://github.com/moononournation/Arduino_GFX) v1.3.7+
- M5StickC / M5StickC-Plus library (for M5Stack boards)

## File Structure

```
espgfx_helloworld/
├── espgfx_helloworld.ino   # Main sketch
└── gfx-helper.h            # Display init, LED flashing, invert toggle
```

## Installation

1. Open `espgfx_helloworld.ino` in Arduino IDE
2. Edit the board define at the top:
    ```c
    // *** BEGIN editing of your settings ...
    #define ARDUINO_M5STICKC
    // *** END editing of your settings ...
    ```
3. Select the correct board and COM port
4. Click **Upload**

The display should show "Hello World!" with resolution info, then start flashing the LED and inverting colors.

## Adding a New Board

Add a new `#elif defined(ARDUINO_YOURBOARD)` block with your pin definitions and Arduino_GFX display/bus constructor. See [espgfxGIF](../espgfxGIF/) for more complete examples with additional boards.
