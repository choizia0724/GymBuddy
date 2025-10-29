#include "NfcReaderUart.h"

NfcReaderUart::NfcReaderUart(const Pins& pins, const Config& cfg)
: pins_(pins),
  cfg_(cfg),
  HSU_(2),             // ESP32-S3: UART2 사용 (원하면 바꿔도 됨)
  hsu_(HSU_),
  nfc_(hsu_) {}

bool NfcReaderUart::begin() {
  // 선택 RST 핀 처리
  if (pins_.rst >= 0) {
    pinMode(pins_.rst, OUTPUT);
    digitalWrite(pins_.rst, HIGH);
    delay(5);
    hwReset();
  }

  // 1차 보레이트로 시도 → 실패 시 2차 보레이트
  if (tryInitAtBaud_(cfg_.baudPrimary)) {
    ready_ = true; return true;
  }
  if (cfg_.baudFallback > 0 && cfg_.baudFallback != cfg_.baudPrimary) {
    if (tryInitAtBaud_(cfg_.baudFallback)) {
      ready_ = true; return true;
    }
  }

  ready_ = false;
  return false;
}

bool NfcReaderUart::tryInitAtBaud_(long baud) {
  // UART 핀/보레이트 설정
  HSU_.end();
  delay(5);
  HSU_.begin(baud, SERIAL_8N1, pins_.rx, pins_.tx);
  delay(20);

  // PN532 시작
  nfc_.begin();
  uint32_t ver = nfc_.getFirmwareVersion();
  if (!ver) return false;

  // 읽기 설정
  nfc_.SAMConfig(); // Normal mode + IRQ 사용 안함
  nfc_.setPassiveActivationRetries(cfg_.passiveRetries);
  return true;
}

bool NfcReaderUart::getFirmware(uint32_t& ver) const {
  if (!ready_) return false;
  ver = const_cast<PN532&>(nfc_).getFirmwareVersion();
  return ver != 0;
}

bool NfcReaderUart::readUID(uint8_t* uid, uint8_t& uidLen) {
  if (!ready_) return false;
  uidLen = 0;
  // ISO14443A, 타임아웃 cfg_.pollMs
  if (nfc_.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, cfg_.pollMs)) {
    return (uidLen > 0);
  }
  return false;
}

void NfcReaderUart::hwReset(uint16_t lowMs, uint16_t waitMs) {
  if (pins_.rst < 0) return;
  digitalWrite(pins_.rst, LOW);
  delay(lowMs);
  digitalWrite(pins_.rst, HIGH);
  delay(waitMs);
}
