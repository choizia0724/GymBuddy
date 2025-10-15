#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_VL53L0X.h>
#include "routes.h"
#include "nfc.h"
#include "laser.h"
#include "power.h"
#include "config.h"
#include "status_led.h"
#include "DistanceSensor.h"

// -------------------- Web / Auth --------------------
WebServer server(8080);
bool isLoggedIn = false;

// -------------------- Laser / Power Pins --------------------

constexpr int LASER_EN_PIN = 20;   // 레이저 EN(PWM) — 10k/20k 분압 뒤 5V 모듈은 3.3V PWM만 인가됨
constexpr int VBAT_ADC_PIN = 8;    // 배터리 전압 ADC

// -------------------- NFC (비 I2C 전제) --------------------

constexpr int NFC_SDA_PIN = 11;
constexpr int NFC_SCL_PIN = 10;

// -------------------- Distance Sensor (VL53L0X via I2C) --------------------

Adafruit_VL53L0X lox;

constexpr int DIS_SDA_PIN   = 29;
constexpr int DIS_SCL_PIN   = 28;

DistanceSensor::Pins pins{DIS_SDA_PIN, DIS_SCL_PIN, -1, -1};

DistanceSensor::Config disCfg{
  .i2cHz = 400000,
  .measureTimeoutMs = 200,
  .touchThresholdMm = 40,
  .medianN = 3
};

DistanceSensor sensor(pins, disCfg);
bool touched = false;
uint32_t lastPrintMs = 0;

// -------------------- Serial CLI: Laser --------------------
namespace {
void handleSerialLaserCommand() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }

  String upper = line;
  upper.toUpperCase();

  if (upper == "LASER ON") {
    Laser::on();
    Serial.println("Laser enabled (duty=" + String(Laser::duty()) + "%, freq=" + String(Laser::freq()) + "Hz)");
    return;
  }

  if (upper == "LASER OFF") {
    Laser::off();
    Serial.println("Laser disabled");
    return;
  }

  if (upper.startsWith("LASER F=")) {
    long freq = upper.substring(8).toInt();
    if (freq >= Laser::MIN_FREQ_HZ && freq <= Laser::MAX_FREQ_HZ) {
      Laser::setFreq(freq);
      Serial.println("Laser frequency set to " + String(freq) + "Hz");
    } else {
      Serial.println("Frequency out of range (" + String(Laser::MIN_FREQ_HZ) + "-" + String(Laser::MAX_FREQ_HZ) + "Hz)");
    }
    return;
  }

  if (upper.startsWith("LASER D=")) {
    long duty = upper.substring(8).toInt();
    if (duty >= 0 && duty <= 100) {
      Laser::setDuty(duty);
      Serial.println("Laser duty set to " + String(duty) + "%");
    } else {
      Serial.println("Duty out of range (0-100%)");
    }
    return;
  }

  Serial.println("Unknown laser command. Use 'LASER ON', 'LASER OFF', 'LASER F=<2000-5000>', 'LASER D=<0-100>'");
}
} // namespace

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
  Serial.println("\n[VL53L0X] init...");

  Wire.begin(DIS_SDA_PIN, DIS_SCL_PIN);    // ESP32는 핀 지정 가능
  Wire.setClock(400000);           // 100k 또는 400k

  // 기본 주소 0x29
  if (!lox.begin()) {
    Serial.println("VL53L0X init failed!");
    while (1) delay(10);
  }
  lox.setTimeout(50); // 라이브러리 타임아웃(옵션)
  Serial.println("VL53L0X ready");

  if (!sensor.begin()) {
    Serial.println("! Sensor init failed. Check wiring/power/XSHUT.");
    for(;;) { delay(1000); }
  }
  Serial.println("OK");

  // --- NFC ---
  Serial.println("\n=== NFC bring-up ===");
  Wire.begin(NFC_SDA_PIN, NFC_SCL_PIN);
  if (!nfc.begin()) {
    Serial.println("PN532 init failed (not I2C; check SPI/UART wiring & config).");
    while (true) { delay(1000); }
  }

  nfc.printFirmware();
  nfc.configureSAM();  // 카드 감지 모드 진입
  Serial.println("Ready. Tap a card/tag...");

}

void loop() {
  // --- HTTP ---
  server.handleClient();

  // --- Serial CLI (Laser) ---
  handleSerialLaserCommand();

  // 작은 양념
  delay(1);

  // --- Distance read & touch event (every ~50ms) ---
  if (millis() - lastPrintMs >= 50) {
    lastPrintMs = millis();

    uint16_t mm;
    if (sensor.read(mm)) {
      Serial.print("DIST(mm): ");
      Serial.println(mm);

      bool nowTouched = (mm <= disCfg.touchThresholdMm); // ← cfg → disCfg로 수정
      if (nowTouched && !touched) Serial.println("[EVENT] TOUCH ✔");
      if (!nowTouched && touched) Serial.println("[EVENT] RELEASE ✖");
      touched = nowTouched;
    } else {
      Serial.println("DIST: -- (out of range or read fail)");
    }
  }

  // --- NFC poll (single try with 200ms timeout) ---
  NfcTag tag;
  if (nfc.pollTag(tag, 200)) {
    Serial.print("Tag detected: ");
    Serial.print(tag.tech);
    Serial.print("  UID: ");
    for (uint8_t i = 0; i < tag.uidLen; i++) {
      if (i) Serial.print(':');
      Serial.printf("%02X", tag.uid[i]);
    }
    Serial.println();
  }
}
