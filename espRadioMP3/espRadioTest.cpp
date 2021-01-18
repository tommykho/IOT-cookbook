#if 0 // Change to 0 to disable this code (must enable ONE user*.cpp only!)

#include <M5StickC.h>
#include <WiFi.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorAAC.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorTalkie.h>
#include <AudioOutputI2S.h>
#include <spiram-fast.h>

//  
//  espRadioTest Version 2020.12b (Source/Buffer Tester)
//  Board: M5StickC (esp32)
//  Author: tommyho510@gmail.com
//  Required: Arduino library ESP8266Audio 1.60
//

// Enter your WiFi, Station, button settings here:
const char *SSID     = "XXXXXXXX";
const char *PASSWORD = "XXXXXXXX";
const int bufferSize = 128 * 1024; // buffer size in byte
const char *URLaac="http://ice4.somafm.com/christmas-32-aac";
const char *URLmp3="http://ice2.somafm.com/christmas-128-mp3";

const int LED = 10;            // GPIO LED
const int BTNA = 37;           // GPIO Play and Pause
const int BTNB = 39;           // GPIO Switch Channel / Volume

AudioGeneratorTalkie *talkie;
AudioGeneratorAAC *aac;
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *fileaac, *filemp3;
AudioFileSourceBuffer *buffaac, *buffmp3;
AudioOutputI2S *out, *outaac, *outmp3;

uint32_t chipID = ESP.getEfuseMac();
uint8_t spREADY[]         PROGMEM = {0x6A,0xB4,0xD9,0x25,0x4A,0xE5,0xDB,0xD9,0x8D,0xB1,0xB2,0x45,0x9A,0xF6,0xD8,0x9F,0xAE,0x26,0xD7,0x30,0xED,0x72,0xDA,0x9E,0xCD,0x9C,0x6D,0xC9,0x6D,0x76,0xED,0xFA,0xE1,0x93,0x8D,0xAD,0x51,0x1F,0xC7,0xD8,0x13,0x8B,0x5A,0x3F,0x99,0x4B,0x39,0x7A,0x13,0xE2,0xE8,0x3B,0xF5,0xCA,0x77,0x7E,0xC2,0xDB,0x2B,0x8A,0xC7,0xD6,0xFA,0x7F};
uint8_t spPAUSE[]         PROGMEM = {0x00,0x00,0x00,0x00,0xFF,0x0F};

void parseChip() {
  Serial.printf("Chip ID:     %d\n", chipID);
  Serial.printf("Chip Rev:    %d\n", ESP.getChipRevision());
  Serial.printf("CPU Freq:    %d\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size:  %d\n", ESP.getFlashChipSize());
  Serial.printf("Flash Speed: %d\n", ESP.getFlashChipSpeed());
  Serial.printf("Total heap:  %d\n", ESP.getHeapSize());
  Serial.printf("Free heap:   %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM:  %d\n\n", ESP.getFreePsram());
}

void initwifi() {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  // Try forever
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("STATUS(Connecting to WiFi) ");
    delay(1000);
    i = i + 1;
    if (i > 10) {
      ESP.restart();
    }
  }
  Serial.println("OK");
}

void talkieReady() {
  out = new AudioOutputI2S(0, 1); // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(0.8);
  talkie = new AudioGeneratorTalkie();
  talkie->begin(nullptr, out);
  talkie->say(spREADY, sizeof(spREADY));
  talkie->say(spPAUSE, sizeof(spPAUSE));
}

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 45, 2);
  M5.Lcd.print(s2);
  M5.Lcd.print("                                                                                          ");
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

void playAAC() {
  outaac = new AudioOutputI2S(0, 1); // Output to builtInDAC
  outaac->SetOutputModeMono(true);
  outaac->SetGain(0.8);
  fileaac = new AudioFileSourceICYStream(URLaac);
  fileaac->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buffaac = new AudioFileSourceBuffer(fileaac, bufferSize);
  buffaac->RegisterStatusCB(StatusCallback, (void*)"buffer");
  aac = new AudioGeneratorAAC();
  aac->RegisterStatusCB(StatusCallback, (void*)"aac");
  aac->begin(buffaac, outaac);
  Serial.printf("STATUS(URL) %s \n", URLaac);
  Serial.flush();
}

void loopAAC() {
  if (aac->isRunning()) {
     if (!aac->loop()) aac->stop();
  } else {
    Serial.printf("Status(Stream) Stopped \n");
    delay(1000);
  }
}

void playMP3() {
  outmp3 = new AudioOutputI2S(0, 1); // Output to builtInDAC
  outmp3->SetOutputModeMono(true);
  outmp3->SetGain(0.8);
  filemp3 = new AudioFileSourceICYStream(URLmp3);
  filemp3->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buffmp3 = new AudioFileSourceBuffer(filemp3, bufferSize);
  buffmp3->RegisterStatusCB(StatusCallback, (void*)"buffer");
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buffmp3, outmp3);
  Serial.printf("STATUS(URL) %s \n", URLmp3);
  Serial.flush();
}

void loopMP3() {
  if (mp3->isRunning()) {
     if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("Status(Stream) Stopped \n");
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED , HIGH);
  pinMode(BTNA, INPUT);
  pinMode(BTNB, INPUT);

  M5.begin();
  M5.Lcd.setRotation(3);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("M5StickC");
  M5.Lcd.println("  Stream Test");

  parseChip();  
  initwifi();
  talkieReady();
  delay(500);
  playAAC();
}

void loop() {
  loopAAC();
}

#endif
