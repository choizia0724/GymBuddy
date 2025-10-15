#include "nfc.h"

#if PN532_USE_I2C
  #include <Wire.h>
  #include <Adafruit_PN532.h>
  static Adafruit_PN532 g_pn532(Wire);

#elif PN532_USE_SPI
  #include <SPI.h>
  #include <Adafruit_PN532.h>
  // 하드웨어 SPI: SS만 넘기면 되고, begin에서 핀 바인딩 가능
  static Adafruit_PN532 g_pn532(PN532_SS);

#elif PN532_USE_HSU
  #include <HardwareSerial.h>
  #include <Adafruit_PN532.h>
  static HardwareSerial PN532Serial(1); // UART1 사용 예
  static Adafruit_PN532 g_pn532(PN532Serial);

#else
  #error "Select one transport: PN532_USE_SPI or PN532_USE_HSU or PN532_USE_I2C"
#endif

NfcReader::NfcReader(int irqPin, int rstPin)
: irqPin_(irqPin), rstPin_(rstPin) {}

bool NfcReader::begin() {
#if PN532_USE_I2C
  // 정말 I2C를 쓸 때만 (지금 기본은 SPI)
  Wire.begin(PN532_SDA, PN532_SCL, PN532_I2C_HZ);

#elif PN532_USE_SPI
  // 커스텀 핀 SPI 시작 (하드웨어 SPI)
  SPI.begin(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

#elif PN532_USE_HSU
  // UART 핀과 보레이트 설정
  PN532Serial.begin(PN532_BAUD, SERIAL_8N1, PN532_RX, PN532_TX);
#endif

  g_pn532.begin();

  uint32_t version = g_pn532.getFirmwareVersion();
  return (version != 0);
}

void NfcReader::printFirmware() {
  uint32_t v = g_pn532.getFirmwareVersion();
  if (!v) {
    Serial.println("PN532 firmware not found");
    return;
  }
  uint8_t ic  = (v >> 24) & 0xFF;
  uint8_t ver = (v >> 16) & 0xFF;
  uint8_t rev = (v >>  8) & 0xFF;
  Serial.printf("PN532 IC: 0x%02X  FW: %u.%u\n", ic, ver, rev);
}

void NfcReader::configureSAM() {
  // SAMConfig: Normal mode, timeouts 내부 기본값
  g_pn532.SAMConfig();
}

bool NfcReader::pollTag(NfcTag& out, uint16_t timeoutMs) {
  uint8_t uid[10];
  uint8_t uidLen = 0;

  bool ok = g_pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                        uid, &uidLen, timeoutMs);
  if (!ok) return false;

  memset(&out, 0, sizeof(out));
  strncpy(out.tech, "ISO14443A", sizeof(out.tech) - 1);
  out.uidLen = uidLen;
  memcpy(out.uid, uid, uidLen);
  return true;
}
