#include "NfcReader.h"

static uint8_t castPin(int v) { return (v < 0) ? 0xFF : (uint8_t)v; }

NfcReader::NfcReader(const Pins& pins, const Config& cfg)
: pins_(pins), cfg_(cfg),
  // I2C 생성자: (irq, reset, &Wire)
  pn532_(castPin(pins.irq), castPin(pins.rst), &Wire)
{}

bool NfcReader::begin() {
  // I2C 라인 열기
  Wire.begin(pins_.sda, pins_.scl);
  Wire.setClock(cfg_.i2cHz);

  pn532_.begin();

  // 펌웨어 버전 확인 (0이면 실패)
  uint32_t ver = pn532_.getFirmwareVersion();
  if (!ver) {
    initialized_ = false;
    return false;
  }

  // SAM 설정 (카드 감지 가능 상태)
  pn532_.SAMConfig();

  initialized_ = true;
  lastPollMs_  = millis();
  return true;
}

void NfcReader::configureSAM() {
  pn532_.SAMConfig();                  // Adafruit_PN532 표준 API
}

void NfcReader::printFirmware() {
  if (!initialized_) return;
  uint32_t ver = pn532_.getFirmwareVersion();
  if (!ver) {
    Serial.println("[PN532] No firmware response");
    return;
  }
  uint8_t ic    = (ver >> 24) & 0xFF;
  uint8_t verMaj= (ver >> 16) & 0xFF;
  uint8_t verMin= (ver >>  8) & 0xFF;
  uint8_t support = ver & 0xFF;
  Serial.printf("[PN532] IC:0x%02X  Firmware:%u.%u  Support:0x%02X\n",
                 ic, verMaj, verMin, support);
}


bool NfcReader::pollOnce(NfcTag& out) {
  if (!initialized_) return false;

  // 폴링 주기 간격 제한
  uint32_t now = millis();
  if (now - lastPollMs_ < cfg_.pollIntervalMs) return false;
  lastPollMs_ = now;

  uint8_t uid[8];
  uint8_t uidLen = 0;

  // ISO14443A만 우선 지원
  bool ok = pn532_.readPassiveTargetID(
              PN532_MIFARE_ISO14443A,
              uid, &uidLen,
              cfg_.cmdTimeoutMs /* timeout ms */
            );
  if (!ok) return false;

  // 결과 채우기
  if (uidLen > sizeof(out.uid)) uidLen = sizeof(out.uid);
  memcpy(out.uid, uid, uidLen);
  out.uidLen = uidLen;
  // out.tech는 기본 "ISO14443A" 그대로

  return true;
}

void NfcReader::update() {
  if (!initialized_) return;
  uint32_t now = millis();
  if (now - lastPollMs_ < cfg_.pollIntervalMs) return;
  lastPollMs_ = now;

  NfcTag tag;
  if (pollOnce(tag)) {
    Serial.print("Tag UID: ");
    for (uint8_t i=0;i<tag.uidLen;++i) {
      if (i) Serial.print(':');
      Serial.printf("%02X", tag.uid[i]);
    }
    Serial.print(" tech=");
    Serial.println(tag.tech);
  }
}
