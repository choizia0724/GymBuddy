#pragma once
#include <Arduino.h>

namespace Power {
  void begin(int adcPin, float vref = 3.30f, uint8_t adcBits = 12, float dividerGain = 2.0f);
  float vbat();        // 전압(V)
  bool  isLow(float th = 3.6f);   // 임계치 이하 체크
}
