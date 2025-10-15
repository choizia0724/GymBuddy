#pragma once
#include <Arduino.h>

// ===== Transport 선택 =====
// 한 가지만 1로 두세요. (기본: SPI)
#ifndef PN532_USE_I2C
#define PN532_USE_I2C 0
#endif
#ifndef PN532_USE_SPI
#define PN532_USE_SPI 1
#endif
#ifndef PN532_USE_HSU
#define PN532_USE_HSU 0
#endif

// ===== SPI 핀 (배선에 맞게 수정) =====
#if PN532_USE_SPI
#ifndef PN532_SS
#define PN532_SS   5
#endif
#ifndef PN532_SCK
#define PN532_SCK  6
#endif
#ifndef PN532_MOSI
#define PN532_MOSI 7
#endif
#ifndef PN532_MISO
#define PN532_MISO 4
#endif
#endif

// ===== HSU(UART) 핀 (배선에 맞게 수정) =====
#if PN532_USE_HSU
#ifndef PN532_RX     // ESP32가 수신하는 핀 (PN532 TX 연결)
#define PN532_RX   18
#endif
#ifndef PN532_TX     // ESP32가 송신하는 핀 (PN532 RX 연결)
#define PN532_TX   17
#endif
#ifndef PN532_BAUD
#define PN532_BAUD 115200
#endif
#endif

// ===== I2C 핀(정말 I2C라면만; 지금은 기본 비활성) =====
#if PN532_USE_I2C
#ifndef PN532_SDA
#define PN532_SDA  11
#endif
#ifndef PN532_SCL
#define PN532_SCL  10
#endif
#ifndef PN532_I2C_HZ
#define PN532_I2C_HZ 400000
#endif
#endif

// ===== Tag struct =====
struct NfcTag {
  char     tech[16];
  uint8_t  uid[10];
  uint8_t  uidLen;
};

// ===== NfcReader =====
class NfcReader {
public:
  NfcReader(int irqPin = -1, int rstPin = -1);
  bool begin();
  void printFirmware();
  void configureSAM();
  bool pollTag(NfcTag& out, uint16_t timeoutMs = 200);

private:
  int irqPin_;
  int rstPin_;
};
