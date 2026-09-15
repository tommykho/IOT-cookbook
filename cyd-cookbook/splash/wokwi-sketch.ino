/*
 * espgfxSplash - Wokwi proof of concept
 *
 * Same splash logic as espgfxSplash.ino, retargeted at the Wokwi CYD part
 * (board-esp32-2432s028r):
 *   - ILI9341 240x320 stands in for the 3248S035 ST7796 320x480
 *   - GIF lives in flash, because Wokwi cannot upload a LittleFS image
 *
 * PROJECT FILES NEEDED IN WOKWI
 *   sketch.ino      <- this file
 *   diagram.json    <- provided separately
 *   libraries.txt   <- provided separately
 *   gif_source.h    <- provided separately
 *   logo_gif.h      <- generate with:  xxd -i logo.gif > logo_gif.h
 *   gifdec.h        <- from your IOT-cookbook repo, patched per gif_source.h
 *   gifdec.cpp      <- ditto
 *
 * WHAT THIS PROVES
 *   fade curve, scaling maths, loop counting, timing, menu handoff.
 * WHAT IT DOES NOT PROVE
 *   real pin maps, true SPI throughput, PSRAM behaviour, the AXS15231B
 *   canvas quirk. Simulated SPI is not representative of real framerate.
 */

// ============================================================
//  *** BEGIN editing of your settings ***
// ============================================================

#define BOARD_WOKWI_CYD_2432S028R

#define STAGE_LED     1     // RGB LED as a boot-stage indicator (see note below)

#define SPLASH_LOOPS  1
#define FADE_IN_MS    600
#define FADE_OUT_MS   600
#define SCALE_TO_FIT  1

#define DEBUG

// ============================================================
//  *** END editing of your settings ***
// ============================================================

#include <Arduino_GFX_Library.h>
#include "gif_source.h"
#include "gifdec.h"
#include "logo_gif.h"   // defines logo_gif[] and logo_gif_len

// ------------------------------------------------------------
//  Board configuration
// ------------------------------------------------------------

#if defined(BOARD_WOKWI_CYD_2432S028R)
// Sunton CYD 2.8" - ESP32-WROOM-32, ILI9341, 240x320, HSPI. 4 MB, no PSRAM.
// Panel RST is tied to the board's reset rail, so pass -1.
#define GFX_BL 21
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */, HSPI);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 0 /* rotation */, false /* IPS */);

// Onboard RGB LED. Active LOW - HIGH is off.
#define LED_R 4
#define LED_G 16
#define LED_B 17
#else
#error "This sketch is the Wokwi variant - use espgfxSplash.ino for real hardware."
#endif

// ------------------------------------------------------------
//  Stage indicator
//
//  A fade legitimately renders a black screen, which looks identical to a
//  crash. The RGB LED disambiguates:
//      blue   = booting
//      green  = splash playing
//      red    = splash failed, went straight to menu
//      off    = menu up
// ------------------------------------------------------------

static void stage(bool r, bool g, bool b) {
#if STAGE_LED
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
#endif
}

// ------------------------------------------------------------
//  Scaling state (nearest-neighbour, 16.16 fixed point)
// ------------------------------------------------------------

static uint8_t  *gifFrameBuf = nullptr;
static uint16_t *lineBuf     = nullptr;
static int16_t   dstW, dstH, dstX, dstY;
static uint32_t  xRatio, yRatio;

static void computeScale(uint16_t gifW, uint16_t gifH) {
  int16_t scrW = gfx->width();
  int16_t scrH = gfx->height();

#if SCALE_TO_FIT
  float s = min((float)scrW / gifW, (float)scrH / gifH);
  dstW = (int16_t)(gifW * s);
  dstH = (int16_t)(gifH * s);
#else
  dstW = min((int16_t)gifW, scrW);
  dstH = min((int16_t)gifH, scrH);
#endif

  dstX = (scrW - dstW) / 2;
  dstY = (scrH - dstH) / 2;

  xRatio = ((uint32_t)gifW << 16) / dstW;
  yRatio = ((uint32_t)gifH << 16) / dstH;
}

// ------------------------------------------------------------
//  Push one decoded frame, scaled and faded
// ------------------------------------------------------------

static void pushFrame(uint16_t gifW, uint8_t fade) {
  gfx->startWrite();
  gfx->writeAddrWindow(dstX, dstY, dstW, dstH);

  for (int16_t y = 0; y < dstH; y++) {
    uint16_t sy = (uint16_t)(((uint32_t)y * yRatio) >> 16);
    uint8_t *srcRow = gifFrameBuf + ((uint32_t)sy * gifW * 3);
    uint32_t sx = 0;

    for (int16_t x = 0; x < dstW; x++) {
      uint8_t *p = srcRow + ((sx >> 16) * 3);
      uint16_t r = ((uint16_t)p[0] * fade) >> 8;
      uint16_t g = ((uint16_t)p[1] * fade) >> 8;
      uint16_t b = ((uint16_t)p[2] * fade) >> 8;
      lineBuf[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      sx += xRatio;
    }
    gfx->writePixels(lineBuf, dstW);
  }

  gfx->endWrite();
}

// ------------------------------------------------------------
//  Splash sequence - GIF sourced from flash
// ------------------------------------------------------------

static bool playSplash() {
  // gd_open_gif_mem is the flash-backed opener you add to gifdec.cpp;
  // it is the existing gd_open_gif with gd_src_from_mem() substituted
  // for the file open.
  gd_GIF *gif = gd_open_gif_mem(logo_gif, logo_gif_len);
  if (!gif) {
#ifdef DEBUG
    Serial.println("gd_open_gif_mem failed - is logo_gif.h a valid GIF?");
#endif
    return false;
  }

  computeScale(gif->width, gif->height);

  size_t frameBytes = (size_t)gif->width * gif->height * 3;
  gifFrameBuf = (uint8_t *)malloc(frameBytes);
  lineBuf     = (uint16_t *)malloc((size_t)dstW * 2);

  if (!gifFrameBuf || !lineBuf) {
#ifdef DEBUG
    Serial.printf("alloc failed: need %u B, largest free block %u B\n",
                  (unsigned)frameBytes, (unsigned)ESP.getMaxAllocHeap());
#endif
    free(gifFrameBuf);
    free(lineBuf);
    gd_close_gif(gif);
    return false;
  }

#ifdef DEBUG
  Serial.printf("GIF %ux%u -> %dx%d at (%d,%d), frame buf %u B, blob %u B\n",
                gif->width, gif->height, dstW, dstH, dstX, dstY,
                (unsigned)frameBytes, (unsigned)logo_gif_len);
#endif

  gfx->fillScreen(BLACK);

  uint32_t splashStart = millis();

  for (uint16_t loop = 0; loop < SPLASH_LOOPS; loop++) {
    if (loop > 0) gd_rewind(gif);

    while (gd_get_frame(gif) > 0) {
      uint32_t frameStart = millis();
      gd_render_frame(gif, gifFrameBuf);

      uint32_t elapsed = millis() - splashStart;
      uint8_t fade = (FADE_IN_MS == 0 || elapsed >= FADE_IN_MS)
                       ? 255
                       : (uint8_t)((elapsed * 255UL) / FADE_IN_MS);

      pushFrame(gif->width, fade);

      int32_t budget = (int32_t)(gif->gce.delay * 10) - (int32_t)(millis() - frameStart);
      if (budget > 0) delay(budget);
    }
  }

  uint32_t fadeStart = millis();
  uint32_t e;
  while ((e = millis() - fadeStart) < FADE_OUT_MS) {
    uint8_t fade = 255 - (uint8_t)((e * 255UL) / FADE_OUT_MS);
    pushFrame(gif->width, fade);
  }

  gfx->fillScreen(BLACK);

  free(gifFrameBuf); gifFrameBuf = nullptr;
  free(lineBuf);     lineBuf     = nullptr;
  gd_close_gif(gif);
  return true;
}

// ------------------------------------------------------------

static void mainMenu() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, gfx->height() / 2 - 12);
  gfx->println("Hello World");
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  delay(200);
  Serial.println("\nespgfxSplash - Wokwi POC");
#endif

#if STAGE_LED
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
#endif
  stage(false, false, true);   // blue: booting

  if (!gfx->begin()) {
#ifdef DEBUG
    Serial.println("gfx->begin() failed");
#endif
    while (1) delay(1000);
  }
  gfx->fillScreen(BLACK);

#if GFX_BL > 0
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  stage(false, true, false);   // green: splash playing

  if (!playSplash()) {
    stage(true, false, false); // red: splash failed
#ifdef DEBUG
    Serial.println("splash skipped");
#endif
    delay(500);
  }

  stage(false, false, false);  // off: menu up
  mainMenu();
}

void loop() {
  delay(1000);
}
