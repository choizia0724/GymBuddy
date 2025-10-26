#pragma once
#include <Arduino.h>
#define RT9532_BAT_EN_PIN 19
#endif

namespace Power {
  void begin(int adcPin, float vref = 3.30f, uint8_t adcBits = 12, float dividerGain = 2.0f);
  float vbat();        // 전압(V)
  bool  isLow(float th = 3.6f);   // 임계치 이하 체크
  void configureChargerPin(int batEnPin = RT9532_BAT_EN_PIN);
  bool enableCharging();
  bool disableCharging();
  bool isChargingEnabled();
}
