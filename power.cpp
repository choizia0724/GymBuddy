#include "power.h"

static int    g_adcPin = 1; // 보드 실제 분압 연결 핀로 교체 필요!!
static float  g_vref   = 3.30f;
static float  g_gain   = 2.0f;
static uint8_t g_bits  = 12;

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
