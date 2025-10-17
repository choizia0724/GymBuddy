#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

struct NfcTag {
  uint8_t uid[8]{};
  uint8_t uidLen = 0;
  char    tech[12]{"ISO14443A"};
};

class NfcReader {
public:
  struct Pins {
    int sda;
    int scl;
    int irq;   // -1 이면 미사용 → 255로 캐스팅되어 전달
    int rst;   // -1 이면 미사용 → 255로 캐스팅되어 전달
  };
  struct Config {
    uint32_t i2cHz          = 400000;
    uint16_t pollIntervalMs = 200;
    uint16_t cmdTimeoutMs   = 200;
  };

  NfcReader(const Pins& pins, const Config& cfg);

  bool begin();
  void update();                   
  bool pollOnce(NfcTag& out);      
  void printFirmware();
  void configureSAM();

private:
  Pins pins_;
  Config cfg_;
  bool initialized_ = false;
  uint32_t lastPollMs_ = 0;

  Adafruit_PN532 pn532_;
};
