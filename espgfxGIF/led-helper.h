/*
 * Author: tommyho510@gmail.com
 * Project details: https://github.com/tommykho/IOT-cookbook https://www.hackster.io/tommyho/arduino-animated-gif-player-8964df
*/

#ifndef _LED_H
#define _LED_H

#ifndef LED
#define LED      2
#endif

hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux       = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t TimerCount = 0;

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    digitalWrite(LED, TimerCount % 100);
    TimerCount++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void ledTimer() {
  pinMode(LED, OUTPUT);
  timerSemaphore = xSemaphoreCreateBinary();
  timer          = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 50000, true);
  timerAlarmEnable(timer);
}

#endif /* _LED_H */
