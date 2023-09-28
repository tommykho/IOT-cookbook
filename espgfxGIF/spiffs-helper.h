/*
 * Author: tommyho510@gmail.com
 * Project details: https://github.com/tommykho/IOT-cookbook https://www.hackster.io/tommyho/arduino-animated-gif-player-8964df
*/

#include <SPIFFS.h>
#ifndef _SPIFFS_H
#define _SPIFFS_H

void listSPIFFS() {
  gifArraySize = 0;
  Serial.println("{ls /:[");
  if (SPIFFS.begin(true)) {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      gifArray[gifArraySize] = String(file.name());
      Serial.println(("  {#:" + String(gifArraySize) + ", name:" + String(file.name()) + ",                     ").substring(0,40) + "\tsize:" + String(file.size()) + "}");
      file = root.openNextFile();
      gifArraySize++;
    }
    randGIF_FILENAME = gifArray[random(0, gifArraySize)];
    //p = millis();
    //randGIF_FILENAME = gifArray[millis() % gifArraySize];
    Serial.println("  {randomGIF:" + String(randGIF_FILENAME) +"}");
    Serial.println("]}");
  }
}

void eraseSPIFFS() {
  if(SPIFFS.begin(true)) {
    bool formatted = SPIFFS.format();
    if(formatted) {
      Serial.println("\n\nSuccess formatting");
      listSPIFFS();
    } else {
      Serial.println("\n\nError formatting");
    }
  }
}

void loadSPIFFS() {
  // Init SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println(F("ERROR: SPIFFS mount failed!"));
    gfx->println(F("ERROR: SPIFFS mount failed!"));
  } else {

#ifndef GIF_FILENAME
#define GIF_FILENAME
    playFile = "/" + String(randGIF_FILENAME);
    Serial.println("{Opening random GIF_FILENAME " + playFile + "}");
#else
    playFile = GIF_FILENAME;
    Serial.println("{Opening designated GIF_FILENAME " + playFile + "}");
#endif
    vFile = SPIFFS.open(playFile);
  }
}

#endif /* _SPIFFS_H */