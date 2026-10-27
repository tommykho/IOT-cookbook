/*
 *  cyd-video-player Version 2026.10A
 *  display.h: Arduino_GFX display, LVGL glue and XPT2046 touch
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

extern Arduino_GFX *gfx;

bool displayBegin();             // backlight, SPI bus, panel, touch
void displayMessage(const char *title, const char *detail = nullptr); // plain text screen, no LVGL

// LVGL owns the panel only while active. Slideshow and video draw straight to gfx.
void lvglBegin();
void lvglSuspend();
void lvglResume();               // waits for finger release, then redraws the whole screen
void lvglTask();                 // call from loop(); no-op while suspended

// Touch in screen coordinates (after rotation and calibration)
bool touchRead(int16_t &x, int16_t &y);
bool touchTapped(int16_t &x, int16_t &y); // press edge, polled every TOUCH_POLL_MS
void touchCalibrate(bool force);          // 3-point calibration, saved to NVS
