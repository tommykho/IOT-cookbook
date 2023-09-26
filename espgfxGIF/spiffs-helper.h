#include <SPIFFS.h>

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