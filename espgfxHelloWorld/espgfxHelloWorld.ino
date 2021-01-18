#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */

/*
 *  espgfxHelloWorld Version 2021.01A (Basic Edition)
 *  Board: TTGO T-Display (esp32)
 *  Author: tommyho510@gmail.com
 *  Original Author: moononournation
 *  Required: Arduino library Arduino_GFX 1.0.5
 *  Dependency: 
 */

// *** BEGIN editing of your settings ...
#define ARDUINO_TDISPLAY
// *** END editing of your settings ...

#if defined(ARDUINO_M5STICKC)
/* M5Stack */
/* 0.96" ST7735 IPS LCD 80x160 M5Stick-C * (Rotation: 0 bottom up, 1 right, 2 top, 3 left) */
//#define TFT_MOSI 15
//#define TFT_SCLK 13
//#define TFT_CS   5
//#define TFT_DC   23
#define TFT_RST  18
#define TFT_BL 2
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(23 /* DC */, 5 /* CS */, 13 /* SCK */, 15 /* MOSI */, -1 /* MISO */);
Arduino_ST7735 *gfx = new Arduino_ST7735(bus, TFT_RST /* RST */, 3 /* rotation */, true /* IPS */, 80 /* width */, 160 /* height */, 26 /* col offset 1 */, 1 /* row offset 1 */, 26 /* col offset 2 */, 1 /* row offset 2 */);
const int BTN_A = 37;
const int BTN_B = 39;

#elif defined(ARDUINO_TDISPLAY) 
/* TTGO T-Display */
/* 1.14" ST7789 IPS LCD 135x240 TTGO T-Display (Rotation: 0 bottom, 1 left, 2 top, 3 right) */
//#define TFT_MOSI 19
//#define TFT_SCLK 18
//#define TFT_CS   5
//#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL 4
Arduino_ESP32SPI_DMA *bus = new Arduino_ESP32SPI_DMA(16 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RST /* RST */, 1 /* rotation */, true /* IPS */, 135 /* width */, 240 /* height */, 53 /* col offset 1 */, 40 /* row offset 1 */, 52 /* col offset 2 */, 40 /* row offset 2 */);
const int BTN_A = 0;
const int BTN_B = 35;

#endif /* not selected specific hardware */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

void setup(void) {
    gfx->begin();
    gfx->fillScreen(BLACK);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif
    gfx->setCursor(10, 10);
    gfx->setTextColor(RED);
    gfx->println("Hello World!");
    delay(3000); // 3 seconds
}

void loop() {
    gfx->setCursor(random(gfx->width()), random(gfx->height()));
    gfx->setTextColor(random(0xffff));
    gfx->setTextSize(random(9) /* x scale */, random(9) /* y scale */, random(3) /* pixel_margin */);
    gfx->println("Hello World!");
    delay(1000); // 1 second
}
