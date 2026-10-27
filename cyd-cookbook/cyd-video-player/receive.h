/*
 *  cyd-video-player Version 2026.10A
 *  receive.h: Wi-Fi upload (SoftAP, captive portal, QR code, upload handler)
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 */
#pragma once
#include <Arduino.h>
#include "storage.h"

void receiveStart();  // AP + DNS + web server, loads the QR screen (call with audio stopped)
bool receiveLoop();   // call from loop(); true when done, cancelled or idle timed out
void receiveStop();   // AP and server off, Wi-Fi memory released

int receiveCount();             // files saved this session
MediaType receiveLastType();    // type of the last file saved
String receiveLastPath();       // path of the last file saved
String receiveFirstPhotoPath(); // first photo saved this session (slideshow starts there)
