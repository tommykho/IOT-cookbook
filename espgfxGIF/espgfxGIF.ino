/*
 *  espgfxGIF Version 2023.09B (Brightness Edition)
 *  Board: T-Display S3, T-Display, M5StickC-Plus, M5StickC (esp32)
 *  Author: tommyho510@gmail.com
 *  Adapted from: moononournation
 *  Project details: https://github.com/tommykho/IOT-cookbook https://www.hackster.io/tommyho/arduino-animated-gif-player-8964df
 *  Required: Arduino library Arduino_GFX 1.3.7
 *  Dependency: gifdec.h
 *  IOT-cookbook Helpers: spiffs, gfx
 *
 *  Please upload SPIFFS data with ESP32 Sketch Data Upload:
 *  https://github.com/me-no-dev/arduino-esp32fs-plugin
 *  GIF src: various
 */

#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */

// *** BEGIN editing of your settings ...
#define ARDUINO_M5STICKCPLUS
//#define GIF_FILENAME "/your_file.gif" /* comment out for random GIF */
//#define DEBUG /* uncomment this line to start with screen test */
// *** END editing of your settings ...

#if defined(ARDUINO_M5STICKCPLUS)
/* M5Stack */
/* 1.14" ST7789 IPS LCD 135x240 M5StickC Plus * (Rotation: 0 bottom up, 1 right, 2 top, 3 left) */
#define TFT_MOSI 15
#define TFT_SCLK 13
#define TFT_CS   5
#define TFT_DC   23
#define TFT_RST  18
#define LED      10
#define TFT_BL   2
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */,
                      53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);
const int BTN_A = 37;
const int BTN_B = 39;
#include <M5StickCPlus.h>

#elif defined(ARDUINO_M5STICKC) 
/* M5Stack */
/* 0.96" ST7735 IPS LCD 80x160 M5StickC * (Rotation: 0 bottom up, 1 right, 2 top, 3 left) */
#define TFT_MOSI 15
#define TFT_SCLK 13
#define TFT_CS   5
#define TFT_DC   23
#define TFT_RST  18
#define LED      10
#define TFT_BL   2
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, -1 /* MISO */);
Arduino_ST7735 *gfx = new Arduino_ST7735(bus, TFT_RST /* RST */, 3 /* rotation */, true /* IPS */, 80 /* width */, 160 /* height */,
                      26 /* col offset 1 */, 1 /* row offset 1 */, 26 /* col offset 2 */, 1 /* row offset 2 */);
const int BTN_A = 37;
const int BTN_B = 39;
#include <M5StickC.h>

#elif defined(ARDUINO_TDISPLAYS3)
/* LILYGO T-Display */
/* 1.90" ST7789V IPS LCD 170x320 TTGO T-Display (Rotation: 0 bottom, 1 left, 2 top, 3 right) */
#define TFT_CS   6
#define TFT_DC   7
#define TFT_RST  5
#define TFT_BL   38
#define LED      2
Arduino_DataBus *bus = new Arduino_ESP32LCD8(TFT_DC /* DC */, TFT_CS /* CS */, 8 /* WR */, 9 /* RD */, 
					39 /* D0 */, 40 /* D1 */, 41 /* D2 */, 42 /* D3 */,
					45 /* D4 */, 46 /* D5 */, 47 /* D6 */, 48 /* D7 */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 0 /* rotation */, true /* IPS */, 170 /* width */, 320 /* height */,
					35 /* col offset 1 */, 0 /* row offset 1 */, 35 /* col offset 2 */, 0 /* row offset 2 */);
const int BTN_A = 0;
const int BTN_B = 14;

#elif defined(ARDUINO_TDISPLAY) 
/* TTGO T-Display */
/* 1.14" ST7789 IPS LCD 135x240 TTGO T-Display (Rotation: 0 top up, 1 left, 2 bottom, 3 right) */
//#define TFT_MOSI 19
//#define TFT_SCLK 18
//#define TFT_CS   5
//#define TFT_DC   16
#define TFT_RST -1
#define TFT_BL 4
Arduino_DataBus *bus = new Arduino_ESP32SPI(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 52 /* col offset 1 */, 40 /* row offset 1 */, 53 /* col offset 2 */, 40 /* row offset 2 */);
const int BTN_A = 0;
const int BTN_B = 35;

#elif defined(ARDUINO_D1MINI) 
/* LOLIN D1 Mini esp8266 + TFT-2.4 Shield */
/* 2.8" ILI9341 TFT LCD 240x320 (Rotation: 0 top up, 1 left, 2 bottom, 3 right) */
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS   16
#define TFT_DC   15
#define TFT_RST  5
//#define TFT_BL NC
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, TFT_MISO /* MISO */);
Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, TFT_RST /* RST */, 0 /* rotation */, false /* IPS */);
const int BTN_A = 3;
const int BTN_B = 4;

#elif defined(ARDUINO_DEVKITV1) 
/* DOIT ESP32 DEVKIT V1 + ILI9341 */
/* 2.8" ILI9341 TFT LCD 240x320 (Rotation: 0 top up, 1 left, 2 bottom, 3 right) */
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
//#define TFT_BL 4
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, TFT_MISO /* MISO */);
Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, TFT_RST /* RST */, 0 /* rotation */, false /* IPS */);
const int BTN_A = 13;
const int BTN_B = 15;

#endif /* not selected specific hardware */

// Sleep timer
//#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
//#define TIME_TO_SLEEP  1          /* Time ESP32 will go to sleep (in seconds) */
//RTC_DATA_ATTR int bootCount = 0;

// Rotation & Brightness control
int rot[2] = {1, 3};
int backlight[5] = {10, 30, 60, 120, 240};
int axp[5] = {20, 40, 60, 80, 100};
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
byte a = 1;
byte b = 5;
unsigned long p = 0;
bool inv = 0;
int pressA = 0;
int pressB = 0;

String gifArray[30], randGIF_FILENAME, playFile, gifFile;
int gifArraySize;
File vFile;

// Load IOT-cookbook helpers
#include "gifdec.h"  
#include "spiffs-helper.h"
#include "gfx-helper.h"
#include "led-helper.h"

// Main subroutine  
void gfxPlayGIF() {
#if defined(_SPIFFS_H)
  loadSPIFFS();
#endif

  if (!vFile || vFile.isDirectory()) {
    Serial.println(F("ERROR: Failed to open file for reading"));
    gfx->println(F("ERROR: Failed to open file for reading"));
    gfx->println(playFile);
  } else {
    gd_GIF *gif = gd_open_gif(&vFile);
    if (!gif) {
      Serial.println(F("gd_open_gif() failed!"));
    } else {
      int32_t s = gif->width * gif->height;
      uint8_t *buf = (uint8_t *)malloc(s);
      if (!buf) {
        Serial.println(F("buf malloc failed!"));
      } else {
        Serial.println(F("{acion:play, GIF:started, Info:["));
        Serial.printf("  {canvas size: %ux%u}\n", gif->width, gif->height);
        Serial.printf("  {number of colors: %d}\n", gif->palette->size);
        Serial.println(F("]}"));
        
        int t_fstart, t_delay = 0, t_real_delay, res, delay_until;
        int duration = 0, remain = 0;
        while (1) {
          gfx->setAddrWindow((gfx->width() - gif->width) / 2, (gfx->height() - gif->height) / 2, gif->width, gif->height);
          t_fstart = millis();
          t_delay = gif->gce.delay * 10;
          res = gd_get_frame(gif, buf);
          if (res < 0) {
            Serial.println(F("ERROR: gd_get_frame() failed!"));
            break;
          } else if (res == 0) {
            Serial.printf("{action:rewind, duration:%d, remain:%d (%0.1f%%)}\n", duration, remain, 100.0 * remain / duration);
            duration = 0;
            remain = 0;
            gd_rewind(gif);
            continue;
          }

          gfx->startWrite();
          gfx->writeIndexedPixels(buf, gif->palette->colors, s);
          gfx->endWrite();

          t_real_delay = t_delay - (millis() - t_fstart);
          duration += t_delay;
          remain += t_real_delay;
          delay_until = millis() + t_real_delay;
          do {
            delay(1);
          } while (millis() < delay_until);

          adjBrightness();
          adjGIF();
        }
        Serial.println(F("action:stop, GIF:ended"));
        Serial.printf("{duration: %d, remain: %d (%0.1f %%)}\n", duration, remain, 100.0 * remain / duration);
        gd_close_gif(gif);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT);

  Serial.println("{Device:Started}");
#if defined(ARDUINO_M5STICKC)
  M5.begin();
#endif
#if defined(ARDUINO_M5STICKCPLUS)
  M5.begin();
  M5.Beep.tone(4000);
  delay(250);
  M5.Beep.mute();
#endif

#if defined(_LED_H)
  ledTimer();
#endif

#if defined(_SPIFFS_H)
  listSPIFFS();
  //eraseSPIFFS();
#endif
     
  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

  // Turn on Backlight
#ifdef TFT_BL
  //M5.Axp.ScreenBreath(b);
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution); // 5 kHz PWM, 8-bit resolution
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);             // assign TFT_BL pin to channel
  ledcWrite(pwmLedChannelTFT, backlight[b]);           // brightness 0 - 255
#endif

#ifdef DEBUG
  gfxScreenTest();
#endif
 
  gfxPlayGIF();

  // Turn off Backlight
#ifdef TFT_BL
  delay(60000);
  ledcDetachPin(TFT_BL);
#endif

  // Put device to sleep
  gfx->displayOff();
  /*
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.flush(); 
   */
  esp_deep_sleep_start();
}

void loop() {
}
