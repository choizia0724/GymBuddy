#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
// 통신
#include "src/net/web/web.h"
#include "src/net/rest/RestSender.h"
// 설정
#include "src/config/config.h"
// #include "src/devices/nfc/NfcReaderSPI.h"
// Up Down 트렌드 감지
#include "src/app/trend/TrendDetector.h"
// laser Cli
#include "src/app/cli/cli_laser.h"
// 물리 기기들
#include "src/devices/laser/laser.h"
#include "src/devices/power/power.h"
#include "src/devices/status_led/status_led.h"
#include "src/devices/distance/DistanceSensor.h"
#include "src/devices/nfc/NfcReaderUart.h"

// -------------------- NFC --------------------

constexpr int NFC_RX_PIN = 10; // ESP32 RX  <- PN532 TX
constexpr int NFC_TX_PIN = 11; // ESP32 TX  -> PN532 RX
constexpr int NFC_RST_PIN = -1; // 별도 제어 없으면 -1

NfcReaderUart::Pins  nfcPins{NFC_RX_PIN, NFC_TX_PIN, NFC_RST_PIN};
NfcReaderUart::Config nfcCfg;

// 전역 리더 인스턴스
NfcReaderUart nfcUart(nfcPins, nfcCfg);

// -------------------- Distance Sensor (VL53L0X via I2C) --------------------

Adafruit_VL53L0X lox;

constexpr int DIS_SDA_PIN   = 36;
constexpr int DIS_SCL_PIN   = 35;
constexpr int PIN_XSHUT     = -1; // 미사용 
constexpr int PIN_INT       = -1; // 미사용

DistanceSensor::Pins pins{DIS_SDA_PIN, DIS_SCL_PIN, PIN_XSHUT, PIN_INT};

DistanceSensor::Config disCfg{
  .i2cHz = 100000,
  .measureTimeoutMs = 200,
  .touchThresholdMm = 40,
  .medianN = 3
};

DistanceSensor distanceSensor(pins, disCfg, Wire);
// -------------------- Trend Detector --------------------
TrendDetector detector; 

uint32_t lastPrintMs = 0;
const uint32_t PRINT_INTERVAL_MS = 50;

// -------------------- RestSender --------------------

RestSender::Config rsCfg{
  .host        = "isluel.iptime.org",   // 또는 EC2 도메인/IP
  .port        = 32869,
  .basePath    = "/api/v2/esp/count",      
  .useHttps    = false,
  .timeoutMs   = 4000,
  .maxRetries  = 2
};

RestSender sender(rsCfg);

// -------------------- Laser / Power Pins --------------------

constexpr int LASER_EN_PIN = 4;   // 레이저 EN(PWM) — 10k/20k 분압 뒤 5V 모듈은 3.3V PWM만 인가됨
constexpr int VBAT_ADC_PIN = 8;    // 배터리 전압 ADC

// ======================================================

void setup() {
  // --- Serial ---
  Serial.begin(115200);
  Serial.setTimeout(50);
  Serial.println("setup");

  // --- Laser PWM ---
  Laser::begin(LASER_EN_PIN, Laser::DEFAULT_FREQ, Laser::DEFAULT_DUTY);

  // --- Power (Battery / Charger) ---
  Power::begin(VBAT_ADC_PIN);
  Power::configureChargerPin();
  if (!Power::enableCharging()) {
    Serial.println("Failed to enable charger during boot");
  }

  // --- Config / Wi-Fi ---
  Config::begin();
  auto cfg = Config::get();

  bool wantSTA = cfg.staSsid.length() > 0;
  WiFi.mode(wantSTA ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(cfg.apSsid.c_str(), cfg.apPass.c_str());
  if (wantSTA) {
    WiFi.begin(cfg.staSsid.c_str(), cfg.staPass.c_str());
  }

  Serial.println("Access Point started at:");
  Serial.println(WiFi.softAPIP());

  // --- LittleFS ---
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, attempting to format...");
    LittleFS.end();
    if (!LittleFS.format()) {
      Serial.println("LittleFS format failed");
      return;
    }
    if (!LittleFS.begin()) {
      Serial.println("LittleFS mount failed after format");
      return;
    }
    Serial.println("LittleFS formatted successfully");
  }
  Serial.println("LittleFS formatted successfully");

  // --- HTTP Routes ---
  WebServerApp::begin();
  Serial.println("setup Routes Successfully");

  // --- Distance Sensor (VL53L0X) ---
  Serial.println("[VL53L0X] init...");

  if (!distanceSensor.begin()) {
    Serial.println("! DistanceSensor init failed. Check power/I2C wiring/XSHUT.");
    while (true) { delay(1000); }
  }

  Serial.println("VL53L0X ready");

 // --- NFC ---
 Serial.println("[INFO] PN532 HSU(UART) init...");
  if (!nfcUart.begin()) {
    Serial.println("! PN532 HSU init failed (DIP=00, RX/TX 교차, 전원 확인)");
  } else {
    uint32_t ver;
    if (nfcUart.getFirmware(ver)) {
      Serial.printf("PN532 FW: 0x%08lX\n", (unsigned long)ver);
    }
  }

}

void loop() {
  // --- Serial CLI (Laser) ---
  //handleSerialLaserCommand();
  delay(1);

  // --- Distance read & touch event (every ~50ms) ---
  const uint32_t now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) return;
  lastPrintMs = now;
  const String deviceId = "GymBuddy-Yeongdeungpo-01";
  const String tagId    = "TestTag-0001";
  const String isoTs   = String((uint32_t)(time(nullptr)));

  uint16_t d;
  if (distanceSensor.read(d)) {
    if (detector.step(d)) {
      const auto& s = detector.state();
      Serial.printf("Send! stata: %s", (s.phase == TrendDetector::Phase::Up) ? "Up" : "Down");
      Serial.print("\n");
      String json =
        String("{\"device_id\":\"") + deviceId +
        "\",\"tag_id\":\"" + tagId +
        "\",\"minDistance\":\"" + String(s.minv) +
        "\",\"maxDistance\":\"" + String(s.maxv) +
        "\",\"ts\":\"" + isoTs +
        "\"}";

      bool ok = sender.post_plain_http(json);
      Serial.println(ok ? "POST OK" : "POST FAIL");
    }

    const auto& s = detector.state();
    Serial.printf("d=%u phase=%d min=%u max=%u\n", d, (int)s.phase, s.minv, s.maxv);
  }

  // --- NFC poll ---
  uint8_t uid[7]; 
  uint8_t uidLen=0;
  if (nfcUart.readUID(uid, uidLen)) {
    Serial.print("[TAG] UID: ");
    for (uint8_t i=0; i<uidLen; ++i) {
      if (uid[i] < 0x10) Serial.print('0');
      Serial.print(uid[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
    delay(500);
  } else {
    delay(50);
  }
}
