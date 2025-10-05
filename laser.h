#pragma once
#include <Arduino.h>

namespace Laser {
  void begin(int pin = 10, uint32_t freqHz = 2000, uint8_t dutyPct = 70);
  void setDuty(uint8_t dutyPct);     // 0~100
  void setFreq(uint32_t freqHz);     // 2~5kHz 권장
  void on();                         // 마지막 듀티 재적용
  void off();                        // 0%
  uint8_t duty(); uint32_t freq();
}
