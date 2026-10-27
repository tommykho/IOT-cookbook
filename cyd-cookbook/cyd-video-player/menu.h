/*
 *  cyd-video-player Version 2026.10A
 *  menu.h: LVGL main menu and audio player screens
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once

void menuBegin();                  // build the screens once, after lvglBegin()
void menuShow(const char *status); // load the main menu with a status line
void audioScreenShow();            // load the audio player (track list refreshed)
