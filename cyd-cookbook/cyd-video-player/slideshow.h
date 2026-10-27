/*
 *  cyd-video-player Version 2026.10A
 *  slideshow.h: JPG/GIF slideshow and photo viewer from /photos
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino.h>

// Rescan /photos and show the first photo (or the one named by path).
// autoAdvance: slideshow (true) or manual photo viewer (false). false if no photos.
bool slideshowBegin(const char *path, bool autoAdvance);
// Call from loop(). Returns true when the user tapped back to the menu.
bool slideshowLoop();
void slideshowEnd();
int photoCount(); // from the last scan
