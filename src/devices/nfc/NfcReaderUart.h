#pragma once
#include <Arduino.h>
#include <PN532.h>       // elechouse/PN532
#include <PN532_HSU.h>   // elechouse/PN532

class NfcReaderUart {
public:
  struct Pins {
    int rx;          // ESP32 RX  <- PN532 TX
    int tx;          // ESP32 TX  -> PN532 RX
    int rst = -1;    // 선택: PN532 RST (없으면 -1)
  };
  struct Config {
    long baudPrimary   = 115200;   // 대부분 기본
    long baudFallback  = 9600;     // 일부 보드 기본
    uint16_t pollMs    = 50;       // read UID 타임아웃
    uint8_t  passiveRetries = 0xFF; // 무한 재시도(페시브)
  };

  explicit NfcReaderUart(const Pins& pins, const Config& cfg);

  // PN532 초기화 (보레이트 자동 탐색 시도)
  bool begin();

  // 펌웨어 버전 조회 (성공 시 ver=0xMMmmPPVV)
  bool getFirmware(uint32_t& ver) const;

  // ISO14443A 태그 1회 폴링
  bool readUID(uint8_t* uid, uint8_t& uidLen);

  // 수동 리셋(핀 제공 시)
  void hwReset(uint16_t lowMs = 10, uint16_t waitMs = 50);

private:
  bool tryInitAtBaud_(long baud);

  Pins   pins_;
  Config cfg_;
  bool   ready_ = false;

  // 순서 중요: HSU -> PN532_HSU -> PN532
  HardwareSerial HSU_;
  PN532_HSU      hsu_;
  PN532          nfc_;
};
