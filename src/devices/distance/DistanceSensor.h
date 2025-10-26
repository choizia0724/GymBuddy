#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

class DistanceSensor {
public:
  struct Pins {
    int sda;
    int scl;
    int xshut; // 미사용 시 -1
    int irq;   // 미사용 시 -1
  };

  struct Config {
    uint32_t i2cHz = 400000;         // I2C 클럭
    uint16_t measureTimeoutMs = 200; // (참고용) Adafruit 라이브러리는 setTimeout 미제공
    uint16_t touchThresholdMm = 40;  // “터치” 판단 임계값
    uint8_t  medianN = 3;            // 1/3/5 권장
  };
  
  DistanceSensor(const Pins& pins, const Config& cfg, TwoWire& bus = Wire);

  bool begin();
  bool read(uint16_t& mm);

private:
  Pins   pins_;
  TwoWire* bus_;
  Config cfg_;
  Adafruit_VL53L0X lox_;
  bool initialized_ = false;

  bool singleRead_(uint16_t& mm);
};
