# espgfxGIF
https://www.hackster.io/projects/8964df
Based on Arduino_GFX and gifdec, espgfxGIF is an Arduino sketch that plays animated GIF on TFT screen of some Arduino Dev modules, mainly esp32 and esp8266.

## Version 2022.05A ***BUG FIXED***
This script works with Arduino_GFX_Library version 1.2.0 (aka GFX library for Arduino, under Library Manager).

### Features:
* Button A - Invert display
* Button B - Adjust Brightness
* Power btn - Reboot and play the designated animated GIF, or a random GIF file in the SPIFFS if the GIF_FILENAME is not defined.
* High performance. If the GIF has no delay, most esp32 module is capable to play 70fps for a 135 x 240 animated GIF.

### DEV Modules tested:

    M5Stack M5Stick-C (esp32 + ST7735)
    TTGO T-Display (esp32 + ST7789)
    D1 mini with 2.4 TFT shield (esp8266 + ILI9341)

### Libraries used:

    https://github.com/moononournation/Arduino_GFX version 1.2.0
    https://github.com/lecram/gifdec (https://github.com/BasementCat/arduino-tft-gif edition)

### Backgrounds:
TFT_eSPI, which is the most common TFT graphic library, supports BMP, and MJPEG/JPEG files via drawBmp() and drawJpeg(). However, due to the way how GIF handles cmap with custom color palettes, drawGIF is not supported (as what I am aware of). Adafruit_GFX also lack support for animated GIF. Color corruption is a common issue.
suatmm_240x135.gif
ezgif.com-gif-maker.gif
Arduino_GFX is a rewritten library from Adafruit_GFX, TFT_eSPI to support various displays with various data bus interfaces. Using gifdec to fill the GIF frames into to display data bus, an animated GIF can be played on the TFT display.
Installations:
(15 minutes setup. 1 minute GIF upload to SPIFFS. 2 minutes to compile and flash)
1. Download esp32fs to your Ardunio IDE from https://github.com/me-no-dev/arduino-esp32fs-plugin
2. Unzip the folder into %USERPROFILE%\Documents\Arduino\tools
3. Restart Arduino IDE.
4. Make sure esp32, esp8266 board info are installed.
5. Make sure M5StickC library is installed.
6. Install "GFX Library for Arduino" (aka Arduino_GFX) from Library Manager.
7. Download the sketch from my github https://github.com/tommykho/IOT-cookbook
8. Extract and create a folder "espgfxGIF" from the zip file.
9. Put your own animated GIF files on the "espgfxGIF/data" folder. Please note most esp32 DEV modules only have 1Mb of SPIFFS. Limit your total file size to 900Kb.
10. Open the "espgfxGIF.ino" with your Arduino IDE.
11. Connect your DEV module to your Arduino IDE. Select the correct COM port.
12. Edit your own settings with the Module and GIF file that you used.

   // *** BEGIN editing of your settings ...
   #define ARDUINO_TDISPLAY
   //#define GIF_FILENAME "/suatmm_240x135.gif" /* comment out for random GIF */
   // *** END editing of your settings ...

13. Select "Tools, ESP32 Sketch Data Upload" to upload the GIF files to the SPIFFS of your DEV Module.14. Click "Upload" to compile and upload the sketch to your DEV Module.
003A.png

*** IMPORTANT ***
If you are not using m5Stack m5StickC or TTGO T-Display, please add your own configuration to the script after line 52. You need to declare your canvas and data-bus class, MOSI, SCLK, CS, DC, RST, BL pins, as well as control button pins
in the configuration. If you need help with the configuration, please leave a comment or visit 
https://github.com/moononournation/Arduino_GFX/wiki/Canvas-Class
https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class

For example,

   #elif defined(ARDUINO_RPI-PICO)
   /* Raspberry Pi Pico with GC9A01 display */
   Arduino_G *output_display = new Arduino_GC9A01(bus, TFT_RST, 0 /* rotation */, true /* IPS */);
   Arduino_GFX *gfx = new Arduino_Canvas(240 /* width */, 240 /* height */, output_display);
   Arduino_DataBus *bus = new Arduino_RPiPicoSPI(27 /* DC */, 17 /* CS */, PIN_SPI0_SCK /* SCK */, PIN_SPI0_MOSI /* MOSI */, PIN_SPI0_MISO /* MISO */, spi0 /* spi */);

### Known bugs:

* Only play a single GIF file in a loop until the power cycle
* Brightness control is not working with M5Stack-C
* Minor artifact with M5Stack-C

### Features in progress:

* Multiple devices sync (playing GIF/MPEG in synchronization)
* Gesture control with IMU
* Play multiple GIF files in a loop
* MP3 or Radio playback (see my other projects)
* BLE connection for wireless GIF upload (similar to Wireless Image Transfer with Circuit Playground Bluefruit and TFT Gizmo)
* IFTTT and MQTT integration for remote power cycle or GIF change
* MJPEG support (live webcam or IP CAM)Multiple
