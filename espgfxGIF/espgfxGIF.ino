#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */
#include "gifdec.h"

/*
 *  espgfxGIF Version 2021.01A (Brightness Edition)
 *  Board: TTGO T-Display & M5Stack M5StickC (esp32)
 *  Author: tommyho510@gmail.com
 *  Original Author: moononournation
 *  Required: Arduino library Arduino_GFX 1.0.5
 *  Dependency: gifdec.h
 *
 *  Please upload SPIFFS data with ESP32 Sketch Data Upload:
 *  https://github.com/me-no-dev/arduino-esp32fs-plugin
 *  GIF src: various
 */

// *** BEGIN editing of your settings ...
#define ARDUINO_TDISPLAY
//#define GIF_FILENAME "/suatmm_240x135.gif" /* comment out for random GIF */
// *** END editing of your settings ...

#if defined(ARDUINO_M5STICKC)
/* M5Stack */
/* 0.96" ST7735 IPS LCD 80x160 M5Stick-C * (Rotation: 0 bottom up, 1 right, 2 top, 3 left) */
#include <M5StickC.h>
//#define TFT_MOSI 15
//#define TFT_SCLK 13
//#define TFT_CS   5
//#define TFT_DC   23
#define TFT_RST  18
#define TFT_BL 2
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(23 /* DC */, 5 /* CS */, 13 /* SCK */, 15 /* MOSI */, -1 /* MISO */);
Arduino_ST7735 *gfx = new Arduino_ST7735(bus, TFT_RST /* RST */, 1 /* rotation */, true /* IPS */, 80 /* width */, 160 /* height */, 26 /* col offset 1 */, 1 /* row offset 1 */, 26 /* col offset 2 */, 1 /* row offset 2 */);
const int BTN_A = 37;
const int BTN_B = 39;

#elif defined(ARDUINO_TDISPLAY) 
/* TTGO T-Display */
/* 1.14" ST7789 IPS LCD 135x240 TTGO T-Display (Rotation: 0 top up, 1 left, 2 bottom, 3 right) */
//#define TFT_MOSI 19
//#define TFT_SCLK 18
//#define TFT_CS   5
//#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL 4
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 3 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);
const int BTN_A = 0;
const int BTN_B = 35;

#endif /* not selected specific hardware */

// Rotation & Brightness control
int rot[2] = {1, 3};
int backlight[5] = {10, 30, 60, 120, 240};
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
byte a = 1;
byte b = 4;
unsigned long p = 0;
bool inv = 0;
int pressA = 0;
int pressB = 0;

String gifArray[30], newGIF_FILENAME, playFile;
int gifArraySize;
  
void adjBrightness() {
  if (digitalRead(BTN_B) == 0) {
    if (pressB == 0) {
      pressB = 1;
      b++;
      //if (b > 15) b = 7;
      //M5.Axp.ScreenBreath(b);
      if (b >= 5) b = 0;
      ledcWrite(pwmLedChannelTFT, backlight[b]);
      Serial.println("{BTN_B:Pressed, Brightness:" + String(backlight[b]) + "}");
    }
  } else pressB = 0;
}

void adjGIF() {
  if (digitalRead(BTN_A) == 0) {
    if (pressA == 0) {
      pressA = 1;
      inv = !inv;
      gfx->invertDisplay(inv);
      Serial.println("{BTN_A:Pressed, Inverted:" + String(inv) + "}");
      //a++;
      //if (a >= 2)
      //  a = 0;
      //gfx->setRotation(rot[a]);
      //Serial.println("{BTN_A:Pressed, Rotation:" + String(rot[a]) + "}");
      //Serial.println("{BTN_A:Pressed}");
      listSPIFFS();
    }
  } else pressA = 0;
}

void listSPIFFS() {
  gifArraySize = 0;
  Serial.println("{ls /:[");
  if (SPIFFS.begin(true)) {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      gifArray[gifArraySize] = String(file.name());
      Serial.println(("  {#:" + String(gifArraySize) + ", name:" + String(file.name()) + ",                     ").substring(0,40) + "\tsize:" + String(file.size()) + "}");
      file = root.openNextFile();
      gifArraySize++;
    }
    newGIF_FILENAME = gifArray[random(0, gifArraySize)];
    //p = millis();
    //newGIF_FILENAME = gifArray[millis() % gifArraySize];
    Serial.println("  {randomGIF:" + String(newGIF_FILENAME) +"}");
    Serial.println("]}");
  }
}

void eraseSPIFFS() {
  if(SPIFFS.begin(true)) {
    bool formatted = SPIFFS.format();
    if(formatted) {
      Serial.println("\n\nSuccess formatting");
      listSPIFFS();
    } else {
      Serial.println("\n\nError formatting");
    }
  }
}
    
void gfxPlayGIF() {
  // Init SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  } else {

#ifndef GIF_FILENAME
#define GIF_FILENAME
playFile = newGIF_FILENAME;
#else
playFile = GIF_FILENAME;
#endif
    
    File vFile = SPIFFS.open(playFile);
    if (!vFile || vFile.isDirectory()) {
      Serial.println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
      gfx->println(F("ERROR: Failed to open "GIF_FILENAME" file for reading"));
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
          Serial.println(F("{acion:play, GIF:started, Media:"GIF_FILENAME", Info:["));
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
}

void setup() {
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT);

  Serial.begin(115200);
  delay(500);
  Serial.println("{Device:Started,}");

  listSPIFFS();
  //eraseSPIFFS();
     
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

  gfxPlayGIF();

  // Turn off Backlight
#ifdef TFT_BL
  delay(60000);
  ledcDetachPin(TFT_BL);
#endif

  // Put device to sleep
  gfx->displayOff();
  esp_deep_sleep_start();
}

void loop() {
}
