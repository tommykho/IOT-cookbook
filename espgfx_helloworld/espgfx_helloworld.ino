#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */

/*
 *  espgfx_HelloWorld Version 2023.09A (Basic Edition)
 *  Board: TTGO T-Display, M5StickC, M5StickC-Plus (esp32)
 *  Author: tommyho510@gmail.com
 *  Original Author: moononournation
 *  Required: Arduino library Arduino_GFX 1.3.7
 *  Dependency: 
 */

// *** BEGIN editing of your settings ...
#define ARDUINO_M5STICKC
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
//#define TFT_BL   2
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 3 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */,
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
//#define TFT_BL   2
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, -1 /* MISO */);
Arduino_ST7735 *gfx = new Arduino_ST7735(bus, TFT_RST /* RST */, 3 /* rotation */, true /* IPS */, 80 /* width */, 160 /* height */,
                      26 /* col offset 1 */, 1 /* row offset 1 */, 26 /* col offset 2 */, 1 /* row offset 2 */);
const int BTN_A = 37;
const int BTN_B = 39;
#include <M5StickC.h>

#elif defined(ARDUINO_TDISPLAYS3)
/* LILYGO T-Display */
/* 1.90" ST7789V IPS LCD 170x320 TTGO T-Display (Rotation: 0 bottom, 1 left, 2 top, 3 right) */
//#define TFT_MOSI 8
//#define TFT_SCLK 12
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
/* LILYGO T-Display */
/* 1.14" ST7789 IPS LCD 135x240 TTGO T-Display (Rotation: 0 bottom, 1 left, 2 top, 3 right) */
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   4
#define LED      2
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC /* DC */, TFT_CS /* CS */, TFT_SCLK /* TFT_SCLK */, TFT_MOSI /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */,
                      53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);
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
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

#include "gfx-helper.h"

int i = 1;
int j = 1;

void setup(void) {
  init();
  gfx_init();
  gfx_printText("Hello World!\n\nRes: " + String(gfx->width()) + "x" + String(gfx->height())
    + "\nRot: " + String(gfx->getRotation()) );

  delay(3000); // 3 seconds
}

void loop() {
  i = led_flashing(i);
  j = gfx_invert(j);
  delay(1000);
}
