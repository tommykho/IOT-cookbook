# Uncanny Eyes for TTGO T-Display

Version 2021.10a (Basic Edition) | Author: tommyho510@gmail.com
Original Authors: Ian Parker / Phil Burgess (Adafruit)

Animated electronic eyes for the TTGO T-Display (ESP32) using the TFT_eSPI library. Based on the [Adafruit Uncanny Eyes](https://github.com/adafruit/Uncanny_Eyes) project, adapted for the ESP32 T-Display's built-in 135x240 TFT screen.

## Features

- **30 swappable eye styles** — from realistic human eyes to sci-fi and fantasy designs
- **Autonomous blinking** — natural random blink timing
- **Pupil tracking** — eyelids follow pupil position
- **Smooth iris animation** — filtered iris size changes
- **Wink button** — GPIO 35 triggers a wink

## Eye Styles Available

Uncomment one `#include` line in the sketch to select an eye style:

| Category | Styles |
|----------|--------|
| Human | mdefaultEye, defaultEye, doeEye, MyEyeHuman1, MyEyeHuman2 |
| Animal | catEye, goatEye, newtEye, owlEye, dragonEye, ChameleonX_Eye, Lamprey |
| Sci-Fi | terminatorEye, Human-HAL9000, AperatureRed |
| Fantasy | BlueFlameEye, Nebula, NebulaBlueGreen, NebulaPinkViolet, SpiralGalaxy |
| Biohazard | Biohazard-Green_EL, Biohazard-Red_EL |
| Other | clowneye, naugaEye, noScleraEye, faceeye, face, Umbrella, MyEye |

## Required Libraries

- [TFT_eSPI](https://github.com/Xinyuan-LilyGO/TTGO-T-Display/tree/master/TFT_eSPI) — custom version for TTGO T-Display

## Board Configuration

In Arduino IDE, select:
- **Board**: ESP32 Dev Module
- **PSRAM**: Disable
- **Flash Size**: 4MB

## File Structure

```
tdisplay-uncanny-eyes/
├── tdisplay-uncanny-eyes.ino   # Main sketch
├── graphics/                    # 30 eye style header files
│   ├── mdefaultEye.h           # Default human-ish hazel eye
│   ├── catEye.h
│   ├── terminatorEye.h
│   ├── ...                     # (30 .h files total)
│   └── logo.h                  # Startup logo
└── README.md
```

## Installation

1. Install the [TTGO T-Display TFT_eSPI library](https://github.com/Xinyuan-LilyGO/TTGO-T-Display/tree/master/TFT_eSPI)
2. Open `tdisplay-uncanny-eyes.ino` in Arduino IDE
3. Select the eye style by uncommenting the desired `#include "graphics/xxxEye.h"` line
4. Select ESP32 Dev Module, Disable PSRAM, 4MB Flash
5. Click **Upload**

## Pin Configuration

| Pin | Function |
|-----|----------|
| GPIO 16 | Display Data/Command (DC) |
| GPIO 23 | Display Reset |
| GPIO 5 | Chip Select (LEFT eye) |
| GPIO 35 | Wink / Blink button |

## Credits

- [Adafruit Uncanny Eyes](https://github.com/adafruit/Uncanny_Eyes) — Phil Burgess / Paint Your Dragon (MIT license)
- [TTGO T-Display Eyes](https://github.com/Mystereon/TTGO-TDISPLAY-EYES) — Mystereon
- [ESP8266 Uncanny Eyes](https://github.com/Bodmer/ESP8266_uncannyEyes) — Bodmer
- Inspired by David Boccabella's (Marcwolf) hybrid servo/OLED eye concept
