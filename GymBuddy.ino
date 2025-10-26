#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
// 통신
#include "src/net/routes/routes.h"
#include "src/net/rest/RestSender.h"
// 설정
#include "src/config/config.h"
#include "src/devices/nfc/NfcReaderSPI.h"
// Up Down 트렌드 감지
#include "src/app/trend/TrendDetector.h"
// laser Cli
#include "src/app/cli/cli_laser.h"
// 물리 기기들
#include "src/devices/laser/laser.h"
#include "src/devices/power/power.h"
#include "src/devices/status_led/status_led.h"
#include "src/devices/distance/DistanceSensor.h"


// -------------------- Web / Auth --------------------
WebServer server(8080);
bool isLoggedIn = false;

// -------------------- NFC --------------------

constexpr int NFC_SCK  = 12;
constexpr int NFC_MISO = 13;
constexpr int NFC_MOSI = 11;
constexpr int NFC_SS   = 10;
constexpr int NFC_RST  = 9;   // optional, 없으면 -1
constexpr int NFC_IRQ  = 14;  // optional, 없으면 -1

NfcReaderSPI2::Pins NFC_PINS{
  NFC_SCK, NFC_MISO, NFC_MOSI, NFC_SS, NFC_RST, NFC_IRQ
};

NfcReaderSPI2::Config NFC_CFG{
  1000000 // 1MHz
};

// 전역 리더 인스턴스
NfcReaderSPI2 nfc(NFC_PINS, NFC_CFG);

// -------------------- Distance Sensor (VL53L0X via I2C) --------------------

TwoWire disWire = TwoWire(0);
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

DistanceSensor distanceSensor(pins, disCfg, disWire);
// -------------------- Trend Detector --------------------
TrendDetector detector; 

uint32_t lastPrintMs = 0;
const uint32_t PRINT_INTERVAL_MS = 50;

// -------------------- RestSender --------------------

RestSender::Config rsCfg{
  .host        = "localhost",   // 또는 EC2 도메인/IP
  .port        = 8080,
  .basePath    = "/count",      
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
  setupRoutes(server, isLoggedIn);
  Serial.println("setup Routes Successfully");
  server.begin();
  Serial.println("server begin");

  // --- Distance Sensor (VL53L0X) ---
  Serial.println("[VL53L0X] init...");

  if (!distanceSensor.begin()) {
    Serial.println("! DistanceSensor init failed. Check power/I2C wiring/XSHUT.");
    while (true) { delay(1000); }
  }

  Serial.println("VL53L0X ready");

 // --- NFC ---

  Serial.print("MOSI Pin: ");
  Serial.println(MOSI);
  Serial.print("MISO Pin: ");
  Serial.println(MISO);
  Serial.print("SCK Pin: ");
  Serial.println(SCK);
  Serial.print("SS Pin: ");
  Serial.println(SS);  

  Serial.println(F("[PN532] init... (SPI2)"));
  if (!nfc.begin()) {
    Serial.println(F("! PN532 init failed. Check SPI2 wiring/mode/CS."));
    while (true) { delay(1000); }
  }

  uint32_t ver = nfc.firmwareVersion();
  Serial.print(F("PN532 firmware: 0x"));
  Serial.println(ver, HEX);
  Serial.println(F("Waiting for ISO14443A card..."));

}

void loop() {
  // --- HTTP ---
  server.handleClient();

  // --- Serial CLI (Laser) ---
  handleSerialLaserCommand();
  delay(1);

  // --- Distance read & touch event (every ~50ms) ---
   const uint32_t now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) return;
  lastPrintMs = now;

  uint16_t d;
  if (distanceSensor.read(d)) {
    if (detector.step(d)) {
      const auto& s = detector.state();
      Serial.printf("Send! stata: %s");
      Serial.print("\n");
      // String json = String("{\"event\":\"pull_release\",\"min\":") + s.minv +
      //               ",\"max\":" + s.maxv + ",\"last\":" + s.last + "}";

      // bool ok = sender.post_plain_http(nullptr, 0, "/events", json);
      // Serial.println(ok ? "POST OK" : "POST FAIL");
    }

    const auto& s = detector.state();
    Serial.printf("d=%u phase=%d min=%u max=%u\n", d, (int)s.phase, s.minv, s.maxv);
  }

  // --- NFC poll (single try with 200ms timeout) ---
  uint8_t uid[7] = {0};
  uint8_t uidLen = 0;

  if (nfc.readOnce(uid, uidLen, 50)) {
    Serial.print(F("UID: "));
    for (uint8_t i = 0; i < uidLen; i++) {
      if (uid[i] < 0x10) Serial.print('0');
      Serial.print(uid[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
    delay(500);
  } else {
    // 타임아웃 → 계속 폴링
    delay(50);
  }
}
