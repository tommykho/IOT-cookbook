# Microcontrollers/IOT Cookbook (Arduino & MicroPython)

This is the code repository for Microcontrollers/IOT cut-and-paste scripts

**Build practical solutions to read and analyse sensor data, make sounds, control lights and motors, display and publish data, and send alerts using popular microcontrollers.**

## Microcontrollers (applies to below but not limited to):
* Adafruit Circuit Playground Classic / Express (ATmega32u4)
* M5Stack UnitV (Kendryte K210)
* M5Stack Basic Core w/ TFT (esp32)
* M5Stack M5 Faces w/ TFT (esp32)
* M5Stack M5StickC w/ TFT (esp32)
* M5Stack ATOM Matrix (esp32)
* TTGO T-Camera w/ OLED (esp32)
* TTGO T-Display w/ TFT (esp32)
* TTGO T-Journal w/ OLED (esp32)
* Ai-Thinker esp32-cam (esp32)
* Ai-Thinker esp-32S (esp32)
* HELTEC HTIT-WB32 w/ OLED (esp32)
* NodeMCU ESP32 (esp32)
* NodeMCU V3 (esp8266)
* WeMos WiFi-ESP8266 (esp8266)
* WeMos D1 Mini (esp8266)

## Instructions and Navigations
Most of the scripts were either written in:
* [Ardunio](http://https://create.arduino.cc/) (.ino)
* [MicroPython](https://tdicola.github.io/sinobit-micropython/editor/editor.html) (.mpy)
* [M5Flow](http://flow.m5stack.com/) (.m5f)

All codes are organized into folders by project then microcontroller. For example, Blynk/M5StickC or fastLED/M5Atom

Most codes are ready-to-use by simple paste or upload to the microcontroller. The code will look like the following:
```
#-- m5RGB.mpy
from m5stack import *
from m5ui import *
from uiflow import *
import unit

M5Led.on()
rgb0 = unit.get(unit.RGB, unit.PORTA)
title0 = M5Title(title="m5RGB", x=3 , fgcolor=0xFFFFFF, bgcolor=0x0000FF)
...
```

```
#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */
#include "gifdec.h"

/* 
 * espgfxGIF.ino
 */ 
 
/ *** BEGIN editing of your settings ...
#define ARDUINO_TDISPLAY
//#define GIF_FILENAME "/suatmm_240x135.gif" /* comment out for random GIF */
// *** END editing of your settings ...
```

## Related products
* TBD

## Get to Know the Author
I, Tommy Ho, has been coding in some form or other since college, before the web existed and has continued to develop scripts, with a particular passion for Python, R, and Ardunio for over a decade. I have recently expanded my work on automation to microcontroller and robotic projects.

## Suggestions and Feedback
[Click here](mailto://tommyho510@gmail.com) if you have any feedback or suggestions.
