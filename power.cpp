#include "power.h"

namespace {
  constexpr uint32_t kEnablePulseHighUs   = 200;  // Datasheet: 50us~1ms HIGH pulse.
  constexpr uint32_t kEnablePulseLowUs    = 200;  // Datasheet: >50us LOW between pulses.
  constexpr uint8_t  kEnablePulseCount    = 1;    // Single pulse enables charging.
  constexpr uint32_t kDisablePulseHighUs  = 200;  // Datasheet: 50us~1ms HIGH pulse.
  constexpr uint32_t kDisablePulseLowUs   = 200;  // Datasheet: >50us LOW between pulses.
  constexpr uint8_t  kDisablePulseCount   = 2;    // Two consecutive pulses disable charging.
  constexpr uint32_t kPostSequenceDelayUs = 500;  // Allow BAT_EN latch to settle.

  int     g_adcPin            = 1; // 보드 실제 분압 연결 핀로 교체 필요!!
  float   g_vref              = 3.30f;
  float   g_gain              = 2.0f;
  uint8_t g_bits              = 12;
  int8_t  g_batEnPin          = -1;
  bool    g_chargerConfigured = false;
  bool    g_isCharging        = false;

  bool ensureChargerConfigured(const char* action) {
    if (!g_chargerConfigured) {
      Serial.printf("[Power] Charger pin not configured; cannot %s.\n", action);
      return false;
    }
    return true;
  }

  bool sendPulseSequence(uint8_t pulseCount, uint32_t highUs, uint32_t lowUs) {
    if (!ensureChargerConfigured("toggle charger")) {
      return false;
    }

    for (uint8_t i = 0; i < pulseCount; ++i) {
      digitalWrite(g_batEnPin, HIGH);
      delayMicroseconds(highUs);
      digitalWrite(g_batEnPin, LOW);
      if (i + 1 < pulseCount) {
        delayMicroseconds(lowUs);
      }
    }

    delayMicroseconds(kPostSequenceDelayUs);
    return true;
  }
} // namespace

void Power::begin(int adcPin, float vref, uint8_t adcBits, float dividerGain) {
  g_adcPin = adcPin; g_vref = vref; g_bits = adcBits; g_gain = dividerGain;
  analogReadResolution(g_bits);
}

float Power::vbat() {
  uint32_t raw = analogRead(g_adcPin);
  float v = (raw / (float)((1<<g_bits)-1)) * g_vref * g_gain;
  return v;
}

bool Power::isLow(float th) { return vbat() <= th; }


void Power::configureChargerPin(int batEnPin) {
  if (batEnPin < 0) {
    Serial.println("[Power] Invalid BAT_EN pin");
    return;
  }

  g_batEnPin          = batEnPin;
  g_chargerConfigured = true;
  g_isCharging        = false;

  pinMode(g_batEnPin, OUTPUT);
  digitalWrite(g_batEnPin, LOW);

  Serial.printf("[Power] Charger BAT_EN pin configured on GPIO %d\n", g_batEnPin);
}

bool Power::enableCharging() {
  if (!ensureChargerConfigured("enable charging")) {
    return false;
  }

  if (g_isCharging) {
    Serial.println("[Power] Charging already enabled");
    return true;
  }

  if (!sendPulseSequence(kEnablePulseCount, kEnablePulseHighUs, kEnablePulseLowUs)) {
    Serial.println("[Power] Failed to send enable pulse sequence");
    return false;
  }

  g_isCharging = true;
  Serial.println("[Power] Charging enabled");
  return true;
}

bool Power::disableCharging() {
  if (!ensureChargerConfigured("disable charging")) {
    return false;
  }

  if (!g_isCharging) {
    Serial.println("[Power] Charging already disabled");
    return true;
  }

  if (!sendPulseSequence(kDisablePulseCount, kDisablePulseHighUs, kDisablePulseLowUs)) {
    Serial.println("[Power] Failed to send disable pulse sequence");
    return false;
  }

  g_isCharging = false;
  Serial.println("[Power] Charging disabled");
  return true;
}

bool Power::isChargingEnabled() { return g_isCharging; }