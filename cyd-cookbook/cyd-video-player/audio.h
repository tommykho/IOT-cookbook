/*
 *  cyd-video-player Version 2026.10A
 *  audio.h: MP3/WAV player on the internal DAC (GPIO26 speaker)
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino.h>
#include <vector>

void audioBegin();                 // start the decoder task on core 0
void audioRescan();                // reload the /audio track list (only while stopped)
const std::vector<String> &audioTracks();

void audioPlay(const char *path);  // full path or a name in /audio
void audioPlayIndex(int i);
void audioTogglePause();
void audioNext(int dir);           // +1 / -1
void audioStop(bool wait);         // wait = block until the decoder released the arena
void audioSetVolume(uint8_t pct);
uint8_t audioVolume();

bool audioIsActive();              // playing or paused
bool audioIsPaused();
int audioCurrentIndex();           // -1 when stopped
