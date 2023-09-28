/*
 * Author: tommyho510@gmail.com
 * Project details: https://github.com/tommykho/IOT-cookbook https://www.hackster.io/tommyho/arduino-animated-gif-player-8964df
*/

#include <Arduino_GFX_Library.h>  /* Install via Arduino Library Manager */
#ifndef _Arduino_GFX_H
#define _Arduino_GFX_H

// Press BTN_A to invert display
void adjGIF() {
  if (digitalRead(BTN_A) == 0) {
    if (pressA == 0) {
#if defined(ARDUINO_M5STICKCPLUS)
      M5.Beep.tone(3000);
      delay(250);
      M5.Beep.mute();
#endif
      pressA = 1;
      inv = !inv;
      gfx->invertDisplay(inv);
      Serial.println("{BTN_A:Pressed, Inverted:" + String(inv) + "}");
      //a++;
      //if (a >= 2)
      //  a = 0;
      //gfx->setRotation(rot[a]);
      //Serial.println("{BTN_A:Pressed, Rotation:" + String(rot[a]) + "}");
      //Serial.println("{BTN_A:Pressed}");
      listSPIFFS();
    }
  } else {
    pressA = 0;
    M5.Beep.mute();
  }
}

// Press BTN_B to adjust brightness
void adjBrightness() {
  if (digitalRead(BTN_B) == 0) {
    if (pressB == 0) {
      b++;
      if (b >= 5) b = 0; 

#if defined(ARDUINO_M5STICKCPLUS)
      if (b == 4) {
        M5.Beep.tone(2000);
        delay(250);
      } 
      M5.Beep.tone(3000);
      delay(250);
      M5.Beep.mute();
      M5.Axp.ScreenBreath(axp[b]);
#endif

#if defined(ARDUINO_M5STICKC)
      M5.Axp.ScreenBreath(axp[b]);
#endif

      ledcWrite(pwmLedChannelTFT, backlight[b]);
      Serial.println("{BTN_B:Pressed, Brightness:" + String(backlight[b]) + "}");

      pressB = 1;
    }
  } else {
    pressB = 0;
    M5.Beep.mute();
  }
}

unsigned long gfxRainbow(uint8_t cIndex) {
  gfx->fillScreen(BLACK);
  unsigned long start = micros();
  int w = gfx->width(), h = gfx->height(), s = h / 8;
  uint16_t arr [] = { PINK, RED, ORANGE, YELLOW, GREEN, MAGENTA, BLUE, WHITE, PINK, RED, ORANGE, YELLOW, GREEN, MAGENTA, BLUE, WHITE };
  gfx->fillRect(0, 0, w, s, arr [cIndex]);
  gfx->fillRect(0, s, w, 2 * s, arr [cIndex + 1]);
  gfx->fillRect(0, 2 * s, w, 3 * s, arr [cIndex + 2]);
  gfx->fillRect(0, 3 * s, w, 4 * s, arr [cIndex + 3]);
  gfx->fillRect(0, 4 * s, w, 5 * s, arr [cIndex + 4]);
  gfx->fillRect(0, 5 * s, w, 6 * s, arr [cIndex + 5]);
  gfx->fillRect(0, 6 * s, w, 7 * s, arr [cIndex + 6]);
  gfx->fillRect(0, 7 * s, w, 8 * s, arr [cIndex + 7]);
  return micros() - start;
}

unsigned long gfxChar(uint16_t colorT, uint16_t colorB) {
  gfx->fillScreen(colorB);
  unsigned long start = micros();
  gfx->setTextColor(GREEN);
  for (int x = 0; x < 16; x++){
    gfx->setCursor(10 + x * 8, 2);
    gfx->print(x, 16);
  }
  gfx->setTextColor(BLUE);
  for (int y = 0; y < 16; y++){
    gfx->setCursor(2, 12 + y * 10);
    gfx->print(y, 16);
  }

  char c = 0;
  for (int y = 0; y < 16; y++){
    for (int x = 0; x < 16; x++){
      gfx->drawChar(10 + x * 8, 12 + y * 10, c++, colorT, colorB);
    }
  }
  return micros() - start;
}

unsigned long gfxFilledCircles(uint8_t radius, uint16_t color) {
  gfx->fillScreen(BLACK);
  unsigned long start;
  int x, y, r2 = radius * 2,
    w = gfx->width(), h = gfx->height();
  start = micros();
  for(x=radius; x<w; x+=r2) {
    for(y=radius; y<h; y+=r2) {
      gfx->fillCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long gfxCircles(uint8_t radius, uint16_t color) {
  // gfx->fillScreen(BLACK);
  // Screen is not cleared for this one -- this is
  // intentional and does not affect the reported time.
  unsigned long start;
  int x, y, r2 = radius * 2,
    w = gfx->width()  + radius, h = gfx->height() + radius;
  start = micros();
  for(x=0; x<w; x+=r2) {
    for(y=0; y<h; y+=r2) {
      gfx->drawCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

void gfxScreenTest() {
	Serial.print(F("Draw Ranbow: "));
  Serial.println(gfxRainbow(0));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(gfxRainbow(2));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(gfxRainbow(4));
  delay(500);
  Serial.print(F("Draw Ranbow: "));
  Serial.println(gfxRainbow(6));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(gfxFilledCircles(10, MAGENTA));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(gfxCircles(10, BLACK));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(gfxFilledCircles(10, YELLOW));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(gfxCircles(10, BLUE));
  delay(500);
  Serial.print(F("Draw Filled Circles: "));
  Serial.println(gfxFilledCircles(10, RED));
  delay(500);
  Serial.print(F("Draw Circles: "));
  Serial.println(gfxCircles(10, WHITE));
  delay(500);
  Serial.print(F("Draw Text: "));
  Serial.println(gfxChar(WHITE, BLACK));
  delay(500);
  Serial.print(F("Draw Text: "));
  Serial.println(gfxChar(BLUE, WHITE));
  delay(500);
  gfx->fillScreen(BLACK);
}

#endif /* _Arduino_GFX_H */
