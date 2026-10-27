/*
 *  cyd-video-player Version 2026.10A
 *  display.cpp: Arduino_GFX display, LVGL glue and XPT2046 touch
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#include "display.h"
#include "config.h"
#include <Preferences.h>
#if defined(ESP32_2432S028)
#include <TFT_Touch.h> // Touch screen library - add "TFT_Touch_v0.3.zip" from this folder
#endif

// Display global variables (using ESP32SPI with DMA for better performance)
static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO,
    HSPI /* SPI bus - display uses HSPI pins */
);
#if defined(ESP32_3248S035)
Arduino_GFX *gfx = new Arduino_ST7796(bus);
#else
Arduino_GFX *gfx = new Arduino_ILI9341(bus);
#endif

#define LVGL_BUF_LINES 20 // partial render buffer: width x 20 lines x 2 bytes

static bool lvglActive = false;
static lv_display_t *lvDisp = nullptr;

// ---------------------------------------------------------------- Touch driver

struct TouchCal
{
    int16_t xMin, xMax, yMin, yMax; // raw value at screen x=0 / x=width, y=0 / y=height
    uint8_t swap;                   // 1 = raw X runs along the screen's y axis
};
static TouchCal cal = {TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, TOUCH_SWAP_XY};

#if defined(ESP32_3248S035)
// The XPT2046 shares SCK/MOSI/MISO with the display on HSPI. Arduino_GFX keeps a private
// copy of the HAL bus table (with its own mutex), so the two drivers do not lock each other.
// That is safe because every HSPI user (LVGL flush, slideshow, video draw, touch) runs on
// the loop() task. Do not draw or read touch from another task.
static spi_t *touchSpi = nullptr;
static uint32_t touchDiv = 0;

static int16_t bestTwoAvg(int16_t a, int16_t b, int16_t c)
{
    int16_t da = abs(a - b), db = abs(a - c), dc = abs(c - b);
    if (da <= db && da <= dc) return (a + b) >> 1;
    if (db <= da && db <= dc) return (a + c) >> 1;
    return (b + c) >> 1;
}

// Read raw 12-bit X/Y. Each 16-bit transfer returns the result of the previous command.
static bool touchReadRaw(int16_t &rx, int16_t &ry)
{
    if (TOUCH_IRQ >= 0 && digitalRead(TOUCH_IRQ) == HIGH)
        return false; // not pressed, skip the bus entirely
    int16_t d[6];
    spiTransaction(touchSpi, touchDiv, SPI_MODE0, SPI_MSBFIRST);
    digitalWrite(TOUCH_CS, LOW);
    spiTransferByteNL(touchSpi, 0xB1);                       // Z1
    int16_t z1 = spiTransferShortNL(touchSpi, 0xC1) >> 3;    // Z2
    int z = z1 + 4095;
    int16_t z2 = spiTransferShortNL(touchSpi, 0x91) >> 3;    // X
    z -= z2;
    bool down = z >= TOUCH_Z_THRESHOLD;
    if (down)
    {
        spiTransferShortNL(touchSpi, 0x91);                  // first X is noisy, discard
        d[0] = spiTransferShortNL(touchSpi, 0xD1) >> 3;      // X, start Y
        d[1] = spiTransferShortNL(touchSpi, 0x91) >> 3;      // Y, start X
        d[2] = spiTransferShortNL(touchSpi, 0xD1) >> 3;
        d[3] = spiTransferShortNL(touchSpi, 0x91) >> 3;
    }
    d[4] = spiTransferShortNL(touchSpi, 0xD0) >> 3;          // X, last Y + power down (IRQ on)
    d[5] = spiTransferShortNL(touchSpi, 0) >> 3;
    digitalWrite(TOUCH_CS, HIGH);
    spiEndTransaction(touchSpi);
    if (!down)
        return false;
    rx = bestTwoAvg(d[0], d[2], d[4]);
    ry = bestTwoAvg(d[1], d[3], d[5]);
    return true;
}

static void touchBegin()
{
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    if (TOUCH_IRQ >= 0)
        pinMode(TOUCH_IRQ, INPUT);
    // Get the HAL handle for HSPI without attaching pins (gfx->begin attaches them next).
    // SPIClass::begin() would re-attach SCK/MOSI/MISO and stop the display's bus.
    touchSpi = spiStartBus(HSPI, 0, SPI_MODE0, SPI_MSBFIRST);
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 6))
    touchDiv = spiFrequencyToClockDiv(touchSpi, TOUCH_SPI_SPEED); // signature changed in core 3.3.6
#else
    touchDiv = spiFrequencyToClockDiv(TOUCH_SPI_SPEED);
#endif
}
#else
static TFT_Touch tftTouch = TFT_Touch(T_CS, T_CLK, T_DIN, T_DO);

static bool touchReadRaw(int16_t &rx, int16_t &ry)
{
    if (!tftTouch.Pressed())
        return false;
    rx = tftTouch.RawX();
    ry = tftTouch.RawY();
    return true;
}

static void touchBegin() {}
#endif

bool touchRead(int16_t &x, int16_t &y)
{
    int16_t rx, ry;
    if (!touchReadRaw(rx, ry))
        return false;
    int16_t sx = cal.swap ? ry : rx;
    int16_t sy = cal.swap ? rx : ry;
    int32_t w = gfx->width(), h = gfx->height();
    int32_t px = (int32_t)(sx - cal.xMin) * w / (cal.xMax - cal.xMin); // a negative span inverts the axis
    int32_t py = (int32_t)(sy - cal.yMin) * h / (cal.yMax - cal.yMin);
    x = constrain(px, 0, w - 1);
    y = constrain(py, 0, h - 1);
    return true;
}

bool touchTapped(int16_t &x, int16_t &y)
{
    static uint32_t lastPoll = 0;
    static bool wasDown = false;
    uint32_t now = millis();
    if (now - lastPoll < TOUCH_POLL_MS)
        return false;
    lastPoll = now;
    bool down = touchRead(x, y);
    bool tapped = down && !wasDown;
    wasDown = down;
    if (tapped)
        LOGF("Tap at %d,%d\n", x, y);
    return tapped;
}

// Average of a steady press, then wait for release. false on timeout.
static bool waitPress(int16_t &rx, int16_t &ry, uint32_t timeoutMs)
{
    uint32_t start = millis();
    int32_t sx = 0, sy = 0;
    int n = 0;
    while (n < 8)
    {
        if (millis() - start > timeoutMs)
            return false;
        int16_t a, b;
        if (touchReadRaw(a, b))
        {
            sx += a;
            sy += b;
            n++;
        }
        else
        {
            n = 0; sx = 0; sy = 0; // finger lifted, start over
        }
        delay(15);
    }
    rx = sx / n;
    ry = sy / n;
    uint32_t up = millis();
    while (millis() - up < 300) // released for 300 ms
    {
        int16_t a, b;
        if (touchReadRaw(a, b)) up = millis();
        delay(10);
    }
    return true;
}

static void drawCross(int16_t x, int16_t y, uint16_t color)
{
    gfx->drawFastHLine(x - 12, y, 25, color);
    gfx->drawFastVLine(x, y - 12, 25, color);
    gfx->drawCircle(x, y, 6, color);
}

void touchCalibrate(bool force)
{
    Preferences prefs;
    prefs.begin("touch", false);
    if (!force && prefs.getBytesLength("cal") == sizeof(TouchCal))
    {
        prefs.getBytes("cal", &cal, sizeof(TouchCal));
        prefs.end();
        LOGF("Touch calibration loaded: x %d..%d y %d..%d swap %d\n", cal.xMin, cal.xMax, cal.yMin, cal.yMax, cal.swap);
        return;
    }
    const int16_t m = 30, w = gfx->width(), h = gfx->height();
    const int16_t px[3] = {m, (int16_t)(w - m), m};
    const int16_t py[3] = {m, m, (int16_t)(h - m)};
    int16_t rx[3], ry[3];
    for (int attempt = 0; attempt < 3; attempt++)
    {
        bool ok = true;
        for (int i = 0; i < 3 && ok; i++)
        {
            gfx->fillScreen(RGB565_BLACK);
            gfx->setTextColor(RGB565_WHITE);
            gfx->setTextSize(2);
            gfx->setCursor(40, h / 2 - 20);
            gfx->printf("Touch calibration %d/3", i + 1);
            gfx->setTextSize(1);
            gfx->setCursor(40, h / 2 + 10);
            gfx->print("Tap the centre of the cross");
            gfx->setCursor(40, h / 2 + 25);
            gfx->print("(hold BOOT 3 s in the menu to redo)");
            drawCross(px[i], py[i], RGB565_YELLOW);
            ok = waitPress(rx[i], ry[i], TOUCH_CAL_TIMEOUT_MS);
            LOGF("Cal point %d raw %d,%d\n", i, rx[i], ry[i]);
        }
        if (!ok)
        {
            LOGF("Touch calibration timed out, using defaults\n");
            break;
        }
        // Moving right (point 0 -> 1) changes mostly one raw axis: that one is screen x
        TouchCal c;
        c.swap = abs(ry[1] - ry[0]) > abs(rx[1] - rx[0]);
        int16_t sx0 = c.swap ? ry[0] : rx[0], sx1 = c.swap ? ry[1] : rx[1];
        int16_t sy0 = c.swap ? rx[0] : ry[0], sy2 = c.swap ? rx[2] : ry[2];
        float kx = (float)(sx1 - sx0) / (w - 2 * m); // raw per pixel, sign = direction
        float ky = (float)(sy2 - sy0) / (h - 2 * m);
        if (fabsf(kx) * (w - 2 * m) < 400 || fabsf(ky) * (h - 2 * m) < 400)
        {
            LOGF("Calibration points too close, retry\n");
            continue;
        }
        c.xMin = sx0 - kx * m;
        c.xMax = c.xMin + kx * w;
        c.yMin = sy0 - ky * m;
        c.yMax = c.yMin + ky * h;
        cal = c;
        prefs.putBytes("cal", &cal, sizeof(TouchCal));
        LOGF("Touch calibration saved: x %d..%d y %d..%d swap %d\n", cal.xMin, cal.xMax, cal.yMin, cal.yMax, cal.swap);
        break;
    }
    prefs.end();
    gfx->fillScreen(RGB565_BLACK);
}

// ---------------------------------------------------------------- Display

bool displayBegin()
{
    // Set display backlight to High
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, HIGH);

    touchBegin(); // before gfx->begin(), see touchBegin()

    // Display initialization
    LOGF("Display initialization\n");
    if (!gfx->begin(DISPLAY_SPI_SPEED))
    {
        LOGF("Display initialization failed!\n");
        return false;
    }
    gfx->setRotation(DISPLAY_ROTATION);
    gfx->fillScreen(RGB565_BLACK);
    // gfx->invertDisplay(true); // on some cheap yellow models, display must be inverted
#if defined(ESP32_2432S028)
    tftTouch.setRotation(0); // Match display rotation
#endif
    LOGF("Screen size Width=%d,Height=%d\n", gfx->width(), gfx->height());
    return true;
}

void displayMessage(const char *title, const char *detail)
{
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println(title);
    if (detail)
    {
        gfx->setTextSize(1);
        gfx->setCursor(10, 40);
        gfx->println(detail);
    }
}

// ---------------------------------------------------------------- LVGL glue

static uint32_t lvTick() { return millis(); }

static void lvFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, lv_area_get_width(area), lv_area_get_height(area));
    lv_display_flush_ready(disp);
}

static void lvTouch(lv_indev_t *indev, lv_indev_data_t *data)
{
    int16_t x, y;
    if (touchRead(x, y))
    {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvglBegin()
{
    lv_init();
    lv_tick_set_cb(lvTick);
    uint32_t bufBytes = gfx->width() * LVGL_BUF_LINES * sizeof(uint16_t);
    uint8_t *buf = (uint8_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lvDisp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(lvDisp, lvFlush);
    lv_display_set_buffers(lvDisp, buf, nullptr, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvTouch);
    LOGF("LVGL %d.%d.%d, draw buffer %lu bytes, free heap %lu\n",
         lv_version_major(), lv_version_minor(), lv_version_patch(), bufBytes, ESP.getFreeHeap());
}

void lvglSuspend()
{
    lvglActive = false;
}

void lvglResume()
{
    int16_t x, y;
    uint32_t start = millis();
    while (touchRead(x, y) && millis() - start < 3000) // the tap that opened the menu must not click a button
        delay(10);
    lv_indev_reset(nullptr, nullptr);
    lv_obj_invalidate(lv_screen_active());
    lvglActive = true;
    lv_refr_now(lvDisp);
}

void lvglTask()
{
    if (lvglActive)
        lv_timer_handler();
}
