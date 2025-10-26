#include "NfcReaderSPI.h"

// HSPI 전용 SPI 인스턴스 (하나만 공유)
SPIClass NfcReaderSPI2::spi2_(FSPI);

// PN532는 (SS, SPI*) 생성자를 쓰면 됨
NfcReaderSPI2::NfcReaderSPI2(const Pins& pins, const Config& cfg)
: pins_(pins),
  cfg_(cfg),
  pn532_(pins.ss, &spi2_) {}

bool NfcReaderSPI2::begin() {

    // 핀 모드
    if (pins_.rst >= 0) {
    pinMode(pins_.rst, INPUT_PULLUP);

    }
    if (pins_.irq >= 0) {
    pinMode(pins_.irq, INPUT);
    }

    // SPI2 초기화: 기본 SPI.begin() 호출 금지
    spi2_.begin(pins_.sck, pins_.miso, pins_.mosi, pins_.ss);
    // spi2_.begin(pins_.ss);
    delay(2);

    // 라이브러리 내부는 begin()에서 SPI transaction을 관리함
    if (!pn532_.begin()) {
        Serial.println("PN532 begin failed");
        initialized_ = false;
        return false;
    }
    delay(2);
    
    // 펌웨어 버전 확인(선택)
    uint32_t ver = pn532_.getFirmwareVersion();
    if (!ver) {
        Serial.println("Didn't find PN532 board");
        initialized_ = false;
        return false;
    }

    pn532_.SAMConfig(); // 카드 읽기 모드 설정
    initialized_ = true;
    return true;
}

uint32_t NfcReaderSPI2::firmwareVersion() {
  if (!initialized_) return 0;
  return pn532_.getFirmwareVersion();
}

bool NfcReaderSPI2::readOnce(uint8_t* uid, uint8_t& uidLen, uint16_t timeoutMs) {
  if (!initialized_) return false;

  // 타임아웃(ms) 동안 ISO14443A 카드 감지
  bool ok = pn532_.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, timeoutMs);
  return ok;
}
