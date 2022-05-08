#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */
#include "gifdec.h"

/*
 *  espgfxGIF Version 2022.05B (Brightness Edition)
 *  Board: TTGO T-Display & M5Stack M5StickC (esp32)
 *  Author: tommyho510@gmail.com
 *  Original Author: moononournation
 *  Required: Arduino library Arduino_GFX 1.2.1
 *  Dependency: gifdec.h
 *
 *  Please upload SPIFFS data with ESP32 Sketch Data Upload:
 *  https://github.com/me-no-dev/arduino-esp32fs-plugin
 *  GIF src: various
 */

// *** BEGIN editing of your settings ...
#define ARDUINO_TDISPLAY
//#define GIF_FILENAME "/your_file.gif" /* comment out for random GIF */
// *** END editing of your settings ...

//#define DEBUG /* uncomment this line to start with screen test */

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
Arduino_DataBus *bus = new Arduino_ESP32SPI(23 /* DC */, 5 /* CS */, 13 /* SCK */, 15 /* MOSI */, -1 /* MISO */);
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

String gifArray[30], randGIF_FILENAME, playFile;
int gifArraySize;
  
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
    randGIF_FILENAME = gifArray[random(0, gifArraySize)];
    //p = millis();
    //randGIF_FILENAME = gifArray[millis() % gifArraySize];
    Serial.println("  {randomGIF:" + String(randGIF_FILENAME) +"}");
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
    
void gfxPlayGIF() {
  // Init SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  } else {

#ifndef GIF_FILENAME
#define GIF_FILENAME
    playFile = "/" + String(randGIF_FILENAME);
	  Serial.println("{Opening random GIF_FILENAME " + playFile + "}");
#else
    playFile = GIF_FILENAME;
	  Serial.println("{Opening designated GIF_FILENAME " + playFile + "}");
#endif
    
    File vFile = SPIFFS.open(playFile);
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
}

unsigned long testRainbow(uint8_t cIndex) {
  gfx->fillScreen(BLACK);
  unsigned long start = micros();
  int w = gfx->width(), h = gfx->height(), s = h / 8;
  uint16_t arr [] = { PINK, RED, ORANGE, YELLOW, GREEN, MAGENTA, BLUE, WHITE, PINK, RED, ORANGE, YELLOW, GREEN, MAGENTA, BLUE, WHITE };
  gfx->fillRect(0, 0, w, s, arr [cIndex]);
  gfx->fillRect(0, s, w, 2 * s, arr [cIndex + 1]);
  gfx->fillRect(0, 2 * s, w, 3 * s, arr [cIndex + 2]);
  gfx->fillRect(0, 3 * s, w, 4 * s, arr [cIndex + 3]);
  gfx->fillRect(0, 4 * s, w, 5 * s, arr [cIndex + 4]);
  gfx->fillRect(0, 5 * s, w, 6 * s, arr [cIndex + 5]);
  gfx->fillRect(0, 6 * s, w, 7 * s, arr [cIndex + 6]);
  gfx->fillRect(0, 7 * s, w, 8 * s, arr [cIndex + 7]);
  return micros() - start;
}

unsigned long testChar(uint16_t colorT, uint16_t colorB) {
  gfx->fillScreen(colorB);
  unsigned long start = micros();
  gfx->setTextColor(GREEN);
  for (int x = 0; x < 16; x++){
    gfx->setCursor(10 + x * 8, 2);
    gfx->print(x, 16);
  }
  gfx->setTextColor(BLUE);
  for (int y = 0; y < 16; y++){
    gfx->setCursor(2, 12 + y * 10);
    gfx->print(y, 16);
  }

  char c = 0;
  for (int y = 0; y < 16; y++){
    for (int x = 0; x < 16; x++){
      gfx->drawChar(10 + x * 8, 12 + y * 10, c++, colorT, colorB);
    }
  }
  return micros() - start;
}

unsigned long testFilledCircles(uint8_t radius, uint16_t color) {
  gfx->fillScreen(BLACK);
  unsigned long start;
  int x, y, r2 = radius * 2,
    w = gfx->width(), h = gfx->height();
  start = micros();
  for(x=radius; x<w; x+=r2) {
    for(y=radius; y<h; y+=r2) {
      gfx->fillCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long testCircles(uint8_t radius, uint16_t color) {
  // gfx->fillScreen(BLACK);
  // Screen is not cleared for this one -- this is
  // intentional and does not affect the reported time.
  unsigned long start;
  int x, y, r2 = radius * 2,
    w = gfx->width()  + radius, h = gfx->height() + radius;
  start = micros();
  for(x=0; x<w; x+=r2) {
    for(y=0; y<h; y+=r2) {
      gfx->drawCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

void screen_test() {
	Serial.print(F("Draw Ranbow: "));
  Serial.println(testRainbow(0));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(testRainbow(2));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(testRainbow(4));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(testRainbow(6));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(testFilledCircles(10, MAGENTA));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(testCircles(10, BLACK));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(testFilledCircles(10, YELLOW));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(testCircles(10, BLUE));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(testFilledCircles(10, RED));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(testCircles(10, WHITE));
  delay(500);
  Serial.print(F("Draw Text: "));
  Serial.println(testChar(WHITE, BLACK));
  delay(500);
  Serial.print(F("Draw Text: "));
  Serial.println(testChar(BLUE, WHITE));
  delay(500);
  gfx->fillScreen(BLACK);
}

void setup() {
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT);
#if defined(ARDUINO_M5STICKC)
  M5.begin();
#endif
  Serial.begin(115200);
  delay(500);
  Serial.println("{Device:Started}");

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

#ifdef DEBUG
  screen_test();
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
