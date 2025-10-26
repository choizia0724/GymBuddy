#pragma once
#include <stdint.h>

namespace Laser {

  constexpr uint32_t MIN_FREQ_HZ   = 500;    // 하한 (권장: 500Hz 이상)
  constexpr uint32_t MAX_FREQ_HZ   = 20000;  // 상한 (권장: 20kHz 이하)
  constexpr uint8_t  MIN_DUTY_PCT  = 0;      // 0%
  constexpr uint8_t  MAX_DUTY_PCT  = 100;    // 100%

  static constexpr uint32_t DEFAULT_FREQ = 2000; // 2kHz
  static constexpr uint8_t  DEFAULT_DUTY = 70;   // 70%

  void begin(int pin = 10, uint32_t freqHz = DEFAULT_FREQ, uint8_t dutyPct = DEFAULT_DUTY);
  void setDuty(uint8_t dutyPct);   // 0~100%
  void setFreq(uint32_t hz);       // 권장 2~5kHz
  void on();
  void off();
  uint8_t  duty();
  uint32_t freq();
}
