#include <Arduino.h>
#include <driver/ledc.h>
#include "laser.h"

static int g_pin = 10;
static int g_ch  = 0;
static uint8_t  g_dutyPct = Laser::DEFAULT_DUTY;
static uint32_t g_freqHz  = Laser::DEFAULT_FREQ;
static const int RES_BITS = 10; // 0~1023

static void apply() {
  int maxv = (1<<RES_BITS) - 1;
  int val = (g_dutyPct * maxv) / 100;
  ledcWrite(g_ch, val);
}

void Laser::begin(int pin, uint32_t freqHz, uint8_t dutyPct) {
  g_pin = pin; g_freqHz = freqHz; g_dutyPct = dutyPct;
  ledcSetup(g_ch, g_freqHz, RES_BITS);
  ledcAttachPin(g_pin, g_ch);
  apply();
}

void Laser::setDuty(uint8_t dutyPct) { g_dutyPct = constrain(dutyPct, 0, 100); apply(); }
void Laser::setFreq(uint32_t hz)     { g_freqHz  = hz; ledcSetup(g_ch, g_freqHz, RES_BITS); apply(); }
void Laser::on()  { apply(); }
void Laser::off() { ledcWrite(g_ch, 0); }

uint8_t  Laser::duty(){ return g_dutyPct; }
uint32_t Laser::freq(){ return g_freqHz; }
