#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

class NfcReaderSPI2 {
public:
  struct Pins {
    int sck;    // SPI SCK
    int miso;   // SPI MISO
    int mosi;   // SPI MOSI
    int ss;     // SPI CS (SS)
    int rst;    // optional, -1 if unused
    int irq;    // optional, -1 if unused
  };

  struct Config {
    uint32_t spiHz = 1000000; 
  };

  NfcReaderSPI2(const Pins& pins, const Config& cfg);

  bool begin();
  bool readOnce(uint8_t* uid, uint8_t& uidLen, uint16_t timeoutMs = 50);
  uint32_t firmwareVersion(); // 0이면 실패

private:
  Pins pins_;
  Config cfg_;
  static SPIClass spi2_;
  Adafruit_PN532 pn532_;

  bool initialized_ = false;
};
