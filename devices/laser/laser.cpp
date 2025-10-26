#include <Arduino.h>
#include "driver/ledc.h"   // ← ESP-IDF LEDC 드라이버
#include "laser.h"

static int        g_pin      = 10;
static uint32_t   g_freqHz   = Laser::DEFAULT_FREQ; // 예: 2000
static uint8_t    g_dutyPct  = Laser::DEFAULT_DUTY; // 예: 70
static const int  RES_BITS   = 10;                  // 0~1023 (LEDC_TIMER_10_BIT)

// ESP32-S3는 LOW_SPEED 모드 사용
static const ledc_mode_t       MODE   = LEDC_LOW_SPEED_MODE;
static const ledc_timer_t      TIMER  = LEDC_TIMER_0;
static const ledc_channel_t    CH     = LEDC_CHANNEL_0;
static const ledc_timer_bit_t  RES    = LEDC_TIMER_10_BIT;

static inline uint32_t dutyFromPct(uint8_t pct) {
  const uint32_t maxv = (1u << RES_BITS) - 1u;        // 1023
  if (pct > 100) pct = 100;
  return (maxv * pct) / 100u;
}

static void applyDuty() {
  const uint32_t duty = dutyFromPct(g_dutyPct);
  ledc_set_duty(MODE, CH, duty);
  ledc_update_duty(MODE, CH);
}

void Laser::begin(int pin, uint32_t freqHz, uint8_t dutyPct) {
  g_pin     = pin;
  g_freqHz  = freqHz;
  g_dutyPct = dutyPct;

  // 1) 타이머 설정
  ledc_timer_config_t tcfg = {};
  tcfg.speed_mode       = MODE;
  tcfg.timer_num        = TIMER;
  tcfg.duty_resolution  = RES;
  tcfg.freq_hz          = (uint32_t)g_freqHz;
  tcfg.clk_cfg          = LEDC_AUTO_CLK;
  ledc_timer_config(&tcfg);

  // 2) 채널 설정(핀 할당)
  ledc_channel_config_t ccfg = {};
  ccfg.speed_mode     = MODE;
  ccfg.channel        = CH;
  ccfg.timer_sel      = TIMER;
  ccfg.intr_type      = LEDC_INTR_DISABLE;
  ccfg.gpio_num       = (gpio_num_t)g_pin;
  ccfg.duty           = 0;     // 초기 duty
  ccfg.hpoint         = 0;
  ledc_channel_config(&ccfg);

  // 3) 듀티 적용
  applyDuty();
}

void Laser::setDuty(uint8_t dutyPct) {
  g_dutyPct = (dutyPct > 100) ? 100 : dutyPct;
  applyDuty();
}

void Laser::setFreq(uint32_t hz) {
  g_freqHz = hz;
  // 주파수 변경 (타이머에 적용)
  ledc_set_freq(MODE, TIMER, (uint32_t)g_freqHz);
  // 주파수 변경 후에도 현재 듀티 유지 반영
  applyDuty();
}

void Laser::on() {
  applyDuty(); // 마지막 듀티 재적용
}

void Laser::off() {
  ledc_set_duty(MODE, CH, 0);
  ledc_update_duty(MODE, CH);
}

uint8_t  Laser::duty() { return g_dutyPct; }
uint32_t Laser::freq() { return g_freqHz; }
