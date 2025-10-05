#include "ota.h"
#include "config.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

void OTAUpdater::begin() {
  if (WiFi.status() != WL_CONNECTED) return; // STA연결 시에만 사용
  ArduinoOTA
    .onStart([](){ Serial.println("[OTA] Start"); })
    .onEnd([](){ Serial.println("\n[OTA] End"); })
    .onProgress([](unsigned int p, unsigned int t){
      Serial.printf("[OTA] %u%%\r", (p / (t / 100)));
    })
    .onError([](ota_error_t e){ Serial.printf("[OTA] Error %u\n", e); });
  ArduinoOTA.begin();
  Serial.println("[OTA] ready");
}

void OTAUpdater::handle() {
  if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();
}
