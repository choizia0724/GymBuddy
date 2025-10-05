#pragma once
#include <Arduino.h>

namespace Laser {
   constexpr uint32_t MIN_FREQ_HZ   = 2000;
  constexpr uint32_t MAX_FREQ_HZ   = 5000;
  constexpr uint8_t  DEFAULT_DUTY  = 70;
  constexpr uint32_t DEFAULT_FREQ  = 2000;

  void begin(int pin = 10, uint32_t freqHz = DEFAULT_FREQ, uint8_t dutyPct = DEFAULT_DUTY);
  void setDuty(uint8_t dutyPct);     // 0~100
  void setFreq(uint32_t freqHz);     // 2~5kHz 권장
  void on();                         // 마지막 듀티 재적용
  void off();                        // 0%
  uint8_t duty(); uint32_t freq();
}
