#include "NfcReader.h"


static uint8_t castPin(int v) { return (v < 0) ? 0xFF : (uint8_t)v; }

NfcReader::NfcReader(const Pins& pins, const Config& cfg, TwoWire& bus)
: pins_(pins),
  cfg_(cfg),
  bus_(&bus),
  // Adafruit_PN532 I2C 생성자 시그니처: (irq, reset, TwoWire*)
  pn532_(castPin(pins.irq), castPin(pins.rst), bus_)
{}




bool NfcReader::begin() {
  // I2C 라인 열기

  const uint32_t startHz = 100000;
  if (!bus_->begin(pins_.sda, pins_.scl, startHz)) {
    Serial.println("[NFC] I2C begin failed");
    return false;
  }
  // 필요하면 승격 (cfg_.i2cHz가 100kHz 초과일 때만)
  if (cfg_.i2cHz > startHz) {
    bus_->setClock(cfg_.i2cHz);
  }

  // 2) IRQ/RST 핀 설정
  if (pins_.irq >= 0) {
    pinMode(pins_.irq, INPUT_PULLUP); // PN532 IRQ는 대기 HIGH, 준비되면 LOW
  }
  if (pins_.rst >= 0) {
    pinMode(pins_.rst, OUTPUT);
    digitalWrite(pins_.rst, HIGH);
  }

  // 3) PN532 초기화
  pn532_.begin();

  // 4) 펌웨어 버전 확인 (0이면 실패)
  uint32_t ver = pn532_.getFirmwareVersion();
  if (!ver) {
    Serial.println("[NFC] getFirmwareVersion failed (0x48 NACK?)");
    initialized_ = false;
    return false;
  }

  // 5) SAM 설정 (카드 감지 모드)
  pn532_.SAMConfig();

  initialized_ = true;
  lastPollMs_  = millis();
  Serial.println("[NFC] init OK");
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
