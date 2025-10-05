#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "routes.h"
// #include "nfc.h"
#include "laser.h"
#include "power.h"
#include "config.h"
#include "status_led.h"

WebServer server(8080);
bool isLoggedIn = false;
namespace {
  constexpr int LASER_EN_PIN = 10; // GPIO routed to laser EN through a 10k/20k divider so the 5V module only sees ~3.3V PWM.

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
}

void setup() {
  Serial.begin(115200);

  // Laser::begin(LASER_EN_PIN, Laser::DEFAULT_FREQ, Laser::DEFAULT_DUTY);

  Power::configureChargerPin();
  if (!Power::enableCharging()) {
    Serial.println("Failed to enable charger during boot");
  }
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

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  setupRoutes(server, isLoggedIn);
  server.begin();

  StatusLED::begin();
}

void loop() {
  server.handleClient();
  handleSerialLaserCommand();
}
