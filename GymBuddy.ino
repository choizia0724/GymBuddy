#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_VL53L0X.h>
#include "routes.h"
#include "NfcReader.h"
#include "laser.h"
#include "power.h"
#include "config.h"
#include "status_led.h"
#include "DistanceSensor.h"
#include "RestSender.h"

// -------------------- Web / Auth --------------------
WebServer server(8080);
bool isLoggedIn = false;

// -------------------- NFC --------------------

constexpr int NFC_SDA_PIN = 11;
constexpr int NFC_SCL_PIN = 10;
constexpr int NFC_IRQ_PIN = -1;
constexpr int NFC_RST_PIN = -1;


NfcReader::Pins   nfcPins{NFC_SDA_PIN, NFC_SCL_PIN, NFC_IRQ_PIN, NFC_RST_PIN};
NfcReader::Config nfcCfg{
  400000,
  200,
  200};

NfcReader nfc(nfcPins, nfcCfg);

constexpr uint16_t TOUCH_THRESHOLD_MM = 120;

// -------------------- Distance Sensor (VL53L0X via I2C) --------------------

Adafruit_VL53L0X lox;

constexpr int DIS_SDA_PIN   = 36;
constexpr int DIS_SCL_PIN   = 35;
constexpr int PIN_XSHUT = -1; // 미사용 
constexpr int PIN_INT = -1; // 미사용

DistanceSensor::Pins pins{DIS_SDA_PIN, DIS_SCL_PIN, PIN_XSHUT, PIN_INT};

DistanceSensor::Config disCfg{
  .i2cHz = 400000,
  .measureTimeoutMs = 200,
  .touchThresholdMm = 40,
  .medianN = 3
};

DistanceSensor distanceSensor(pins, disCfg);
uint32_t PRINT_INTERVAL_MS = 50;
uint32_t lastPrintMs = 0;
uint32_t minDistanceMm = 1000;
uint32_t maxDistanceMm = 0;
uint32_t lastDistanceMm = 0;
bool startFlag = false;

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
  Serial.println("[VL53L0X] init...");

  if (!distanceSensor.begin()) {
    Serial.println("! DistanceSensor init failed. Check power/I2C wiring/XSHUT.");
    while (true) { delay(1000); }
  }

  Serial.println("VL53L0X ready");

 // --- NFC ---
  Serial.println("\n=== NFC bring-up ===");

  if (!nfc.begin()) {
    Serial.println("[NFC] init failed");
    for(;;) delay(1000);
  }
  nfc.printFirmware();
  nfc.configureSAM();
  Serial.println("Ready. Tap a card/tag...");

}

void loop() {
  // --- HTTP ---
  server.handleClient();

  // --- Serial CLI (Laser) ---
  handleSerialLaserCommand();
  delay(1);

  // --- Distance read & touch event (every ~50ms) ---
  const uint32_t now = millis();
  // if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
  //   // 기지정된 인터벌만큼
  //   lastPrintMs = now;

  //   uint16_t distance_mm;
  //   if (distanceSensor.read(distance_mm)) {
  //     // 거리값 성공
  //     Serial.print("DIST(distance_mm): ");
  //     Serial.println(distance_mm);
  //     Serial.print("DIST(lastDistanceMm): ");
  //     Serial.println(lastDistanceMm);
  //     Serial.print("DIST(minDistanceMm): ");
  //     Serial.println(minDistanceMm);
  //     Serial.print("DIST(maxDistanceMm): ");
  //     Serial.println(maxDistanceMm);

  //     if(lastDistanceMm == 0){
  //       // 초기값 할당
  //       lastDistanceMm = distance_mm;
  //       return;
  //     }

  //     if(lastDistanceMm >= distance_mm &&
  //         startFlag ==false){
  //       startFlag = true;
  //       minDistanceMm = distance_mm;
  //       maxDistanceMm = distance_mm;
  //       lastDistanceMm = distance_mm;
  //       return;
  //     }

  //     if (lastDistanceMm >= distance_mm && 
  //           startFlag == true) {
  //         // 이전 데이터 보다 1cm 이하로 움직이는 것은 무시함
  //         if (distance_mm + 10 < lastDistanceMm) {
  //             return;
  //         }
  //         maxDistanceMm = distance_mm;
  //         lastDistanceMm = distance_mm;
  //     }

  //       // 시작 이지만 계속 내려가면 노이즈 데이터임 시작한거 아님.
  //     if (lastDistanceMm + 10 <= distance_mm && startFlag == true) {
  //         startFlag = false;
  //         //lastDistanceMm = distance_mm;
  //         return;
  //     }

  //     // 종료 check
  //     // 꺾이기 시작하면 꺾이기 시작 바로 전값이 최고 값임.
  //     if (maxDistanceMm + 10 <= distance_mm  && startFlag == true) {
  //       // 현재 데이터가 maxData보다 1cm 보다 더 클 경우만 날때 데이터 전송.
  //       if (distance_mm >= maxDistanceMm + 10) {
  //           // 데이터 전송
           
  //           Serial.println("Send!");
  //           startFlag = false;
  //           lastDistanceMm = distance_mm;
  //       }
  //     }


  //   } else {
  //     // 읽기 실패(범위 밖/타임아웃 등)
  //   }
  // }

  // --- NFC poll (single try with 200ms timeout) ---
  NfcTag tag;
  if (nfc.pollOnce(tag)) {
    Serial.print("[NFC] UID: ");
    for (uint8_t i = 0; i < tag.uidLen; ++i) {
      if (i) Serial.print(':');
      Serial.printf("%02X", tag.uid[i]);
    }
    Serial.print("  tech=");
    Serial.println(tag.tech);
    // TODO: 태그 처리 로직
  }
}
