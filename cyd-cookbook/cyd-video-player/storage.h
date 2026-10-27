/*
 *  cyd-video-player Version 2026.10A
 *  storage.h: SD card, media folders and file naming
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino.h>
#include <vector>
#include "SD.h" // Included with the Espressif Arduino Core (last tested on v3.2.0)

enum MediaType
{
    MEDIA_NONE,
    MEDIA_JPEG,
    MEDIA_GIF,
    MEDIA_MP3,
    MEDIA_WAV,
    MEDIA_MJPEG
};

bool storageBegin(); // mount SD (FAT32) and create the media folders

MediaType mediaTypeOf(const String &name); // by extension
const char *mediaFolder(MediaType t);
uint32_t mediaLimit(MediaType t);
bool isPhoto(MediaType t);
bool isAudio(MediaType t);
bool isVideo(MediaType t);

// File names (not paths) in dir whose type matches, sorted, at most MAX_LIST_FILES
void listMedia(const char *dir, bool (*accept)(MediaType), std::vector<String> &out);
int countMedia(const char *dir, bool (*accept)(MediaType));

String sanitizeName(const String &name);                    // [A-Za-z0-9._-], max 48 chars
String uniquePath(const char *dir, const String &name, const String &stamp);
String formatBytes(size_t bytes);
