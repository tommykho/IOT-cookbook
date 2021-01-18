#include <M5StickC.h>
#include <WiFi.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorTalkie.h>
#include <AudioGeneratorAAC.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioOutputI2SNoDAC.h>
#include <spiram-fast.h>

/*  
 *  m5RadioMP3 Version 2021.01A (MP3 / Voice Edition)
 *  Board: M5StickC (esp32)
 *  Author: tommyho510@gmail.com
 *  Original Author: Milen Penev 
 *  Required: Arduino library ESP8266Audio 1.60
 *  Dependency: TFTTerminal.h
 */

// *** BEGIN editing of your settings ...
#define ARDUINO_M5STICKC
const char *SSID     = "*********";
const char *PASSWORD = "*********";
const int bufferSize = 32 * 1024;  // buffer in byte, default 32 * 1024 = 32kb
char * arrayURL[11] = {
  "http://jenny.torontocast.com:8134/stream",
  "http://ais-edge09-live365-dal02.cdnstream.com/a25710",
  "http://188.165.212.154:8478/stream",
  "https://igor.torontocast.com:1025/;.mp3",
  "http://streamer.radio.co/s06b196587/listen",
  "http://sj32.hnux.com/stream?type=http&nocache=3104", 
  "http://sl32.hnux.com/stream?type=http&nocache=1257", 
  "http://media-ice.musicradio.com:80/ClassicFMMP3",
  "http://naxos.cdnstream.com:80/1255_128",
  "http://149.56.195.94:8015/steam",
  "http://ice2.somafm.com/christmas-128-mp3"
};
String arrayStation[11] = {
  "Mega Shuffle",
  "Orig. Top 40",
  "Way Up Radio",
  "Asia Dream",
  "KPop Way Radio",
  "Smooth Jazz",
  "Smooth Lounge",
  "Classic FM",
  "Lite Favorites",
  "MAXXED Out",
  "SomaFM Xmas"
};
// *** END editing of your settings ...

#if defined(ARDUINO_M5STICKC)
const int LED = 10;                 // GPIO LED 
const int BTN_A = 37;               // GPIO Play and Pause
const int BTN_B = 39;               // GPIO Switch Channel / Volume
const int DAC_OUT = 26;             // GPIO Speaker Out
#elif defined(ARDUINO_TDISPLAY) 
//const int LED;                    // GPIO LED 
const int BTN_A = 0;                // GPIO Play and Pause
const int BTN_B = 35;               // GPIO Switch Channel / Volume
const int DAC_OUT = 26;             // GPIO Speaker Out
#endif /* not selected specific hardware */

// Objects and variables
TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);
AudioGeneratorTalkie *talkie;
//AudioGeneratorAAC *aac;         
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

const int numCh = sizeof(arrayURL)/sizeof(char *);
bool TestMode = false;
uint32_t LastTime = 0;
int playflag = 0;
int ledflag = 0;
//int btnaflag = 0;
//int btnbflag = 0;
float fgain = 1.0;
int sflag = 0;
char *URL = arrayURL[sflag]; 
String station = arrayStation[sflag];
uint8_t spHELLO[]         PROGMEM = {0x00,0xC0,0x80,0x60,0x59,0x08,0x10,0x3D,0xB7,0x00,0x62,0x64,0x3D,0x55,0x4A,0x9E,0x66,0xDA,0xF6,0x56,0xB7,0x3A,0x55,0x76,0xDA,0xED,0x92,0x75,0x57,0xA3,0x88,0xA8,0xAB,0x02,0xB2,0xF4,0xAC,0x67,0x23,0x73,0xC6,0x2F,0x0C,0xF3,0xED,0x62,0xD7,0xAD,0x13,0xA5,0x46,0x8C,0x57,0xD7,0x21,0x0C,0x22,0x4F,0x93,0x4B,0x27,0x37,0xF0,0x51,0x69,0x98,0x9D,0xD4,0xC8,0xFB,0xB8,0x98,0xB9,0x56,0x23,0x2F,0x93,0xAA,0xE2,0x46,0x8C,0x52,0x57,0x66,0x2B,0x8C,0x07};
uint8_t spHELP[]          PROGMEM = {0x08,0xB0,0x4E,0x94,0x00,0x21,0xA8,0x09,0x20,0x66,0xF1,0x96,0xC5,0x66,0xC6,0x54,0x96,0x47,0xEC,0xAA,0x05,0xD9,0x46,0x3B,0x71,0x94,0x51,0xE9,0xD4,0xF9,0xA6,0xB7,0x18,0xB5,0x35,0xB5,0x25,0xA2,0x77,0xB6,0xA9,0x97,0xB1,0xD7,0x85,0xF3,0xA8,0x81,0xA5,0x6D,0x55,0x4E,0x0D,0x00,0xC0,0x00,0x1B,0x3D,0x30,0x00,0x0F};
uint8_t spREADY[]         PROGMEM = {0x6A,0xB4,0xD9,0x25,0x4A,0xE5,0xDB,0xD9,0x8D,0xB1,0xB2,0x45,0x9A,0xF6,0xD8,0x9F,0xAE,0x26,0xD7,0x30,0xED,0x72,0xDA,0x9E,0xCD,0x9C,0x6D,0xC9,0x6D,0x76,0xED,0xFA,0xE1,0x93,0x8D,0xAD,0x51,0x1F,0xC7,0xD8,0x13,0x8B,0x5A,0x3F,0x99,0x4B,0x39,0x7A,0x13,0xE2,0xE8,0x3B,0xF5,0xCA,0x77,0x7E,0xC2,0xDB,0x2B,0x8A,0xC7,0xD6,0xFA,0x7F};
uint8_t spSTOP[]          PROGMEM = {0x0C,0xF8,0xA5,0x4C,0x02,0x1A,0xD0,0x80,0x04,0x38,0x00,0x1A,0x58,0x59,0x95,0x13,0x51,0xDC,0xE7,0x16,0xB7,0x3A,0x75,0x95,0xE3,0x1D,0xB4,0xF9,0x8E,0x77,0xDD,0x7B,0x7F,0xD8,0x2E,0x42,0xB9,0x8B,0xC8,0x06,0x60,0x80,0x0B,0x16,0x18,0xF8,0x7F};
uint8_t spSWITCH[]        PROGMEM = {0x08,0xF8,0x3B,0x93,0x03,0x1A,0xB0,0x80,0x01,0xAE,0xCF,0x54,0x40,0x33,0x99,0x2E,0xF6,0xB2,0x4B,0x9D,0x52,0xA7,0x36,0xF0,0x2E,0x2F,0x70,0xDB,0xCB,0x93,0x75,0xEE,0xA6,0x4B,0x79,0x4F,0x36,0x4C,0x89,0x34,0x77,0xB9,0xF9,0xAA,0x5B,0x08,0x76,0xF5,0xCD,0x73,0xE4,0x13,0x99,0x45,0x28,0x77,0x11,0xD9,0x40,0x80,0x55,0xCB,0x25,0xE0,0x80,0x59,0x2F,0x23,0xE0,0x01,0x0B,0x08,0xA0,0x46,0xB1,0xFF,0x07};
uint8_t spPAUSE[]         PROGMEM = {0x00,0x00,0x00,0x00,0xFF,0x0F};

void Displaybuff() {
    if (TestMode) {
        Disbuff.setTextSize(1);
        Disbuff.setTextColor(TFT_RED);
        Disbuff.drawString("Test Mode", 0, 0, 1);
        Disbuff.setTextColor(TFT_WHITE);
    }
    Disbuff.pushSprite(0, 0);
}

void ColorBar() {
    float color_r, color_g, color_b;
    color_r = 0;
    color_g = 0;
    color_b = 255;

    for (int i = 0; i < 384; i=i+4) {
        if (i < 128) {
            color_r = i * 2;
            color_g = 0;
            color_b = 255 - (i * 2);
        }
        else if ((i >= 128) && (i < 256)) {
            color_r = 255 - ((i - 128) * 2);
            color_g = (i - 128) * 2;
            color_b = 0;
        }
        else if ((i >= 256) && (i < 384)) {
            color_r = 0;
            color_g = 255 - ((i - 256) * 2);
            ;
            color_b = (i - 256) * 2;
            ;
        }
        Disbuff.fillRect(0, 0, 180, 80, Disbuff.color565(color_r, color_g, color_b));
        Displaybuff();
    }

    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            color_r = 0;
            color_g = 0;
            color_b = 0;
            break;
        case 1:
            color_r = 255;
            color_g = 0;
            color_b = 0;
            break;
        case 2:
            color_r = 0;
            color_g = 255;
            color_b = 0;
            break;
        case 3:
            color_r = 0;
            color_g = 0;
            color_b = 255;
            break;
        }
        for (int n = 0; n < 240; n++) {
            color_r = (color_r < 255) ? color_r + 1.0625 : 255U;
            color_g = (color_g < 255) ? color_g + 1.0625 : 255U;
            color_b = (color_b < 255) ? color_b + 1.0625 : 255U;
            Disbuff.drawLine(n, i * 20, n, (i + 1) * 20, Disbuff.color565(color_r, color_g, color_b));
        }
    }
    Displaybuff();
    delay(500);

    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            color_r = 255;
            color_g = 255;
            color_b = 255;
            break;
        case 1:
            color_r = 255;
            color_g = 0;
            color_b = 0;
            break;
        case 2:
            color_r = 0;
            color_g = 255;
            color_b = 0;
            break;
        case 3:
            color_r = 0;
            color_g = 0;
            color_b = 255;
            break;
        }
        for (int n = 0; n < 240; n++) {
            color_r = (color_r > 2) ? color_r - 1.0625 : 0U;
            color_g = (color_g > 2) ? color_g - 1.0625 : 0U;
            color_b = (color_b > 2) ? color_b - 1.0625 : 0U;
            Disbuff.drawLine(159 - n, i * 20, 159 - n, (i + 1) * 20, Disbuff.color565(color_r, color_g, color_b));
        }
    }
    Displaybuff();
    delay(500);
}

void StartPlaying() {
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buff = new AudioFileSourceBuffer(file, bufferSize);
  buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
  out = new AudioOutputI2S(0, 1); // Output to builtInDAC
  out->SetOutputModeMono(true);
//  out->SetGain(0.3);
  out->SetGain(fgain*0.05);
  M5.Lcd.fillTriangle(109, 20, 109 + (5 * fgain), 20, 109 + (5 * fgain), 20 - (2 * fgain), BLUE);
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buff, out);
  Serial.printf("STATUS(URL) %s \n", URL);
  Serial.flush();
}

void StopPlaying() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (buff) {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file) {
    file->close();
    delete file;
    file = NULL;
  }
  Serial.printf("STATUS(Streaming) Stopped\n");
  Serial.flush();
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

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED , HIGH);
  pinMode(BTN_A, INPUT);
  pinMode(BTN_B, INPUT);

  M5.begin();
  M5.Lcd.setRotation(3);

  // Screen Check
  Disbuff.createSprite(160, 80);
  Disbuff.fillRect(0,0,160,80,Disbuff.color565(10,10,10));
  Disbuff.pushSprite(0,0);
  delay(500);
  M5.Axp.ScreenBreath(12);
  ColorBar();

  Disbuff.fillRect(0,0,160,80,TFT_BLACK);
  Disbuff.setTextSize(2);
  Disbuff.drawString("M5StickC", 0, 0, 1);
  Disbuff.drawString("    WebRadio", 0, 35, 1);
  Disbuff.pushSprite(0, 0);
  M5.update();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("M5StickC");
  M5.Lcd.println("    WebRadio");
  delay(1000);
  M5.Lcd.println("WiFi");
  M5.Lcd.println("  Connecting");
  initwifi();

//  StartPlaying();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.print("Ready");

  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 30, 2);
  M5.Lcd.print("Ch ");
  M5.Lcd.print(sflag + 1);
  M5.Lcd.print(" - ");
  M5.Lcd.print(station);
  M5.Lcd.print("          ");

  Serial.printf("STATUS(System) Ready \n\n");
  out = new AudioOutputI2S(0, 1); // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(fgain*0.05);
  talkie = new AudioGeneratorTalkie();
  talkie->begin(nullptr, out);
  talkie->say(spREADY, sizeof(spREADY));
  talkie->say(spPAUSE, sizeof(spPAUSE));
}

void loop() {
  static int lastms = 0;
  if (playflag == 0) {
    if (digitalRead(BTN_A) ==  LOW) {
      StartPlaying();
      playflag = 1;
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(0, 0, 2);
      M5.Lcd.print("Playing");
    }

    if (digitalRead(BTN_B) ==  LOW) {
      sflag = (sflag + 1) % numCh;
      URL = arrayURL[sflag];
      station = arrayStation[sflag];           
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(0, 30, 2);
      M5.Lcd.print("Ch ");
      M5.Lcd.print(sflag + 1);
      M5.Lcd.print(" - ");
      M5.Lcd.print(station);
      M5.Lcd.print("          ");
      talkie->begin(nullptr, out);
      talkie->say(spSWITCH, sizeof(spSWITCH));
      talkie->say(spPAUSE, sizeof(spPAUSE));
      delay (200);
    }
  }

  if (playflag == 1) {
    if (mp3->isRunning()) {
      if (millis() - lastms > 1000) {
        lastms = millis();
        Serial.printf("STATUS(Streaming) %d ms...\n", lastms);

        ledflag = ledflag + 1;
        if (ledflag > 1) {
          ledflag = 0;
          digitalWrite(LED , HIGH);
        } else {
          digitalWrite(LED , LOW);
        }

      }
      if (!mp3->loop()) mp3->stop();
    } else {
      Serial.printf("MP3 done\n");
      playflag = 0;
      digitalWrite(LED , HIGH);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(0, 0, 2);
      M5.Lcd.print("Stop     ");
//      ESP.restart();
    }
    if (digitalRead(BTN_A) ==  LOW) {
      StopPlaying();
      playflag = 0;
      digitalWrite(LED , HIGH);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(0, 0, 2);
      M5.Lcd.print("Stop     ");
      talkie->begin(nullptr, out);
      talkie->say(spSTOP, sizeof(spSTOP));
      talkie->say(spPAUSE, sizeof(spPAUSE));
      M5.Lcd.fillRect(109, 0, 160, 21, BLACK);
      delay(200);
    }
    if (digitalRead(BTN_B) ==  LOW) {
      fgain = fgain + 1.0;
      if (fgain > 10.0) {
        fgain = 1.0;
      }
      out->SetGain(fgain*0.05);
      M5.Lcd.fillRect(109, 0, 160, 21, BLACK);
      M5.Lcd.fillTriangle(109, 20, 109 + (5 * fgain), 20, 109 + (5 * fgain), 20 - (2 * fgain), BLUE);
      Serial.printf("STATUS(Gain) %f \n", fgain*0.05);
      delay(200);
    }
  }
}
