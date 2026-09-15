/*
 * espgfxSplash.ino  -  Version 2026.09A (Splash Edition)
 *
 * Boot splash: play an animated GIF N times with a software fade-in and
 * fade-out, then hand off to the main menu.
 *
 * Derived from espgfxGIF 2022.05B (Brightness Edition) by tommy@tommyho.com
 * Original GIF player author: moononournation
 *
 * Required : Arduino_GFX >= 1.4.3   (NOT 1.2.1 - dev-device configs changed)
 * Depends  : gifdec.h / gifdec.cpp
 * Storage  : LittleFS
 *
 * WHY SOFTWARE FADE
 *   Not every target exposes backlight PWM. Instead of dimming the panel,
 *   each pixel's R/G/B is scaled by a 0-255 fade factor on its way to the
 *   display. Identical visual result, zero hardware dependency.
 *
 * MEMORY NOTE (3248S035 / any WROOM board with no PSRAM)
 *   Full 320x480 framebuffer = 307 KB. Will not fit. This sketch never
 *   allocates one. RAM use is:
 *       gifFrameBuf  = gifW * gifH * 3   (RGB888, gifdec's output)
 *       lineBuf      = dstW * 2          (one destination row, RGB565)
 *   Keep source GIFs small. 240x135 costs ~97 KB and is about the ceiling
 *   on a 4 MB / no-PSRAM board. 160x120 (~58 KB) is a safer starting point.
 */

// ============================================================
//  *** BEGIN editing of your settings ***
// ============================================================

// --- target board: uncomment exactly one ---
#define BOARD_ESP32_2432S028R      // CYD 2.8"  - also the Wokwi part
// #define BOARD_ESP32_3248S035    // CYD 3.5"
// #define BOARD_ESP32_2432S032    // CYD 3.2"
// #define BOARD_JC3248W535C       // Guition 3.5" S3
// #define BOARD_LILYGO_T_DISPLAY_S3
// #define BOARD_M5STICKC_PLUS

// --- where the GIF comes from ---
//   1 = compiled into flash from logo_gif.h  (required for Wokwi and for
//       any board with no SD slot; see gif_source.h)
//   0 = read from LittleFS at GIF_FILENAME
#define GIF_FROM_FLASH  1

#define GIF_FILENAME  "/logo.gif"   // only used when GIF_FROM_FLASH is 0

#define SPLASH_LOOPS  1         // how many times to play the GIF
#define FADE_IN_MS    600       // ramp 0 -> full over this long
#define FADE_OUT_MS   600       // ramp full -> 0 after the last loop
#define SCALE_TO_FIT  1         // 1 = scale + centre, 0 = centre at 1:1

// Replay the splash every N ms instead of stopping at the menu.
// 0 = off (normal firmware behaviour - splash once, then the menu stays).
// Set to a few seconds for a public Wokwi demo, so a visitor who arrives
// mid-run still sees the animation rather than a frozen "Hello World".
#define DEMO_REPLAY_MS 4000

#define DEBUG                   // comment out to silence serial

// ============================================================
//  *** END editing of your settings ***
// ============================================================

#include <Arduino_GFX_Library.h>
#include "gif_source.h"
#include "gifdec.h"

#if GIF_FROM_FLASH
#include "logo_gif.h"      // xxd -i logo.gif > logo_gif.h
#else
#include <LittleFS.h>
#endif

// ------------------------------------------------------------
//  Board configuration
//
//  Pin maps below mirror Arduino_GFX's own Arduino_GFX_dev_device.h.
//  If a board misbehaves, cross-check there first - that header is the
//  authoritative source and is updated with the library.
// ------------------------------------------------------------

#if defined(BOARD_ESP32_2432S028R)
// CYD 2.8" - ESP32-WROOM-32, ILI9341, 240x320, HSPI. 4 MB, no PSRAM.
// This is the board Wokwi emulates (board-esp32-2432s028r), so the same
// build runs in the simulator and on real hardware.
// Panel RST is tied to the board reset rail - pass -1.
#define GFX_BL 21
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */, HSPI);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 0 /* rotation */, false /* IPS */);
// Onboard RGB LED, active LOW.
#define LED_R 4
#define LED_G 16
#define LED_B 17

#elif defined(BOARD_ESP32_3248S035)
// CYD 3.5" - ESP32-WROOM-32, ST7796, 320x480, HSPI. No PSRAM.
#define GFX_BL 27
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */, HSPI);
Arduino_GFX *gfx = new Arduino_ST7796(bus, -1 /* RST */, 0 /* rotation */, true /* IPS */);

#elif defined(BOARD_ESP32_2432S032)
// CYD 3.2" - ESP32-WROOM-32, ST7789, 240x320, HSPI. No PSRAM.
#define GFX_BL 27
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */, HSPI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, -1 /* RST */, 0 /* rotation */, true /* IPS */);

#elif defined(BOARD_JC3248W535C)
// Guition 3.5" - ESP32-S3-WROOM-1, AXS15231B, 320x480, QSPI. 8 MB PSRAM.
// This panel is unreliable with direct writes; it wants a canvas + flush().
// With 8 MB PSRAM a full canvas is affordable here, unlike the CYDs.
#define GFX_BL -1
#define NEEDS_CANVAS_FLUSH 1
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* CS */, 47 /* SCLK */, 21 /* D0 */, 48 /* D1 */, 40 /* D2 */, 39 /* D3 */);
Arduino_GFX *panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED /* RST */, 0, false, 320, 480);
Arduino_GFX *gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 0);

#elif defined(BOARD_LILYGO_T_DISPLAY_S3)
// ESP32-S3, ST7789, 170x320, 8-bit parallel.
// WARNING: ESP32LCD8 was removed in arduino-esp32 v3.0. Either pin the core
// to 2.x, or swap Arduino_ESP32LCD8 for Arduino_ESP32PAR8 below.
#define GFX_BL 38
Arduino_DataBus *bus = new Arduino_ESP32LCD8(
    7 /* DC */, 6 /* CS */, 8 /* WR */, 9 /* RD */,
    39 /* D0 */, 40 /* D1 */, 41 /* D2 */, 42 /* D3 */,
    45 /* D4 */, 46 /* D5 */, 47 /* D6 */, 48 /* D7 */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 5 /* RST */, 0, true, 170, 320, 35, 0, 35, 0);

#elif defined(BOARD_M5STICKC_PLUS)
// ESP32-PICO, ST7789, 135x240, VSPI.
#define GFX_BL -1
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    23 /* DC */, 5 /* CS */, 13 /* SCK */, 15 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 18 /* RST */, 0, true, 135, 240, 52, 40, 53, 40);

#else
#error "No board selected - define one of the BOARD_* macros above."
#endif

// ------------------------------------------------------------
//  Scaling state (nearest-neighbour, 16.16 fixed point)
// ------------------------------------------------------------

static uint8_t  *gifFrameBuf = nullptr;   // RGB888, GIF native size
static uint16_t *lineBuf     = nullptr;   // RGB565, one destination row
static int16_t   dstW, dstH, dstX, dstY;
static uint32_t  xRatio, yRatio;

static void computeScale(uint16_t gifW, uint16_t gifH) {
  int16_t scrW = gfx->width();
  int16_t scrH = gfx->height();

#if SCALE_TO_FIT
  // Largest integer-free scale that fits both axes, aspect preserved.
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
//  Push one decoded frame, scaled and faded, straight to the panel.
//  fade: 0 = black, 255 = full brightness.
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

#ifdef NEEDS_CANVAS_FLUSH
  gfx->flush();
#endif
}

// ------------------------------------------------------------
//  Splash sequence
// ------------------------------------------------------------

static bool playSplash() {
#if GIF_FROM_FLASH
  // gd_open_gif_mem is your existing gd_open_gif with the file open
  // swapped for gd_src_from_mem() - see gif_source.h.
  gd_GIF *gif = gd_open_gif_mem(logo_gif, logo_gif_len);
#else
  gd_GIF *gif = gd_open_gif(GIF_FILENAME);
#endif
  if (!gif) {
#ifdef DEBUG
    Serial.println("gd_open_gif failed");
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
  Serial.printf("GIF %ux%u -> %dx%d at (%d,%d), frame buf %u B\n",
                gif->width, gif->height, dstW, dstH, dstX, dstY,
                (unsigned)frameBytes);
#endif

  gfx->fillScreen(BLACK);
#ifdef NEEDS_CANVAS_FLUSH
  gfx->flush();
#endif

  // --- play loops, fading in across the first FADE_IN_MS ---
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

      // gifdec delay is in centiseconds; subtract time already spent.
      int32_t budget = (int32_t)(gif->gce.delay * 10) - (int32_t)(millis() - frameStart);
      if (budget > 0) delay(budget);
    }
  }

  // --- fade out by re-pushing the final frame at falling brightness ---
  uint32_t fadeStart = millis();
  uint32_t e;
  while ((e = millis() - fadeStart) < FADE_OUT_MS) {
    uint8_t fade = 255 - (uint8_t)((e * 255UL) / FADE_OUT_MS);
    pushFrame(gif->width, fade);
  }

  gfx->fillScreen(BLACK);
#ifdef NEEDS_CANVAS_FLUSH
  gfx->flush();
#endif

  free(gifFrameBuf); gifFrameBuf = nullptr;
  free(lineBuf);     lineBuf     = nullptr;
  gd_close_gif(gif);
  return true;
}

// ------------------------------------------------------------
//  Proof-of-concept stand-in for the real menu
// ------------------------------------------------------------

static void mainMenu() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, gfx->height() / 2 - 12);
  gfx->println("Hello World");
#ifdef NEEDS_CANVAS_FLUSH
  gfx->flush();
#endif
}

// ------------------------------------------------------------

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  delay(200);
  Serial.println("\nespgfxSplash 2026.09A");
#endif

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

#if GIF_FROM_FLASH
  if (!playSplash()) {
#ifdef DEBUG
    Serial.println("splash skipped");
#endif
  }
#else
  if (!LittleFS.begin(false)) {
#ifdef DEBUG
    Serial.println("LittleFS mount failed - upload the filesystem image first");
#endif
  } else if (!playSplash()) {
#ifdef DEBUG
    Serial.println("splash skipped");
#endif
  }
#endif

  mainMenu();
}

void loop() {
#if DEMO_REPLAY_MS > 0
  delay(DEMO_REPLAY_MS);
  playSplash();
  mainMenu();
#else
  delay(1000);
#endif
}
