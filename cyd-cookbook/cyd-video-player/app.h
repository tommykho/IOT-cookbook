/*
 *  cyd-video-player Version 2026.10A
 *  app.h: app states and the shared media arena
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino.h>
#include "config.h"

enum AppState
{
    STATE_SLIDESHOW,    // auto-advancing photos, tap opens the menu
    STATE_MENU,         // LVGL main menu
    STATE_PHOTO_VIEW,   // manual photos: tap left/right edge = prev/next, centre = menu
    STATE_AUDIO_PLAYER, // LVGL track list and controls
    STATE_RECEIVE,      // Wi-Fi AP + QR code + upload web page
    STATE_VIDEO         // MJPEG player, tap opens the menu
};

// Switch state at the next loop() pass. path (optional) selects the file to open first.
void appRequestState(AppState s, const char *path = nullptr);

// Shared media arena: the two MJPEG frame buffers are allocated once at boot (no PSRAM,
// so they must be grabbed before the heap fragments) and lent out when no video plays.
//   arenaAudio(): MP3 decoder state while audio plays, upload write buffer in Receive
//   arenaPhoto(): JPEG or GIF decoder object in the slideshow
extern uint8_t *mjpeg_buf[2];
inline uint8_t *arenaAudio() { return mjpeg_buf[0]; }
inline uint8_t *arenaPhoto() { return mjpeg_buf[1]; }

extern volatile bool skipRequested; // BOOT button pressed (set in ISR)
