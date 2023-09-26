#include <Arduino_GFX_Library.h> 

void init() {
  delay(500);
  Serial.begin(115200);
#ifdef LED
  pinMode(LED, OUTPUT);
#endif
#if defined(ARDUINO_M5STICKC)
  M5.begin();
#endif
#if defined(ARDUINO_M5STICKCPLUS)
  M5.begin();
#endif
}

void gfx_init() {
  gfx->begin();
  gfx->setRotation(1);
  gfx->fillScreen(BLACK);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
}

void gfx_printText(String gfxMessage) {
  gfx->setCursor(5, 5);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->println(gfxMessage);
}

void gfx_printTextRand(String gfxMessage){
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff));
  gfx->setTextSize(random(9) /* x scale */, random(9) /* y scale */, random(3) /* pixel_margin */);
  gfx->println(gfxMessage);
}

unsigned int led_flashing(int i) {
  digitalWrite(LED , i);
  return abs(i - 1);
}

unsigned int gfx_invert(int i){
  gfx->invertDisplay(i);
  return abs(i - 1);
}