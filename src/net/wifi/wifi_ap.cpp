#include "wifi_ap.h"
#include "src/config/config.h"
#include <WiFi.h>

namespace {
  void startAP(const AppConfig& cfg) {
    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(cfg.apSsid.c_str(), cfg.apPass.c_str());
    Serial.printf("[AP] %s (%s)\n", ok ? "started" : "failed", cfg.apSsid.c_str());
    Serial.print("[AP] IP: "); Serial.println(WiFi.softAPIP());
  }
}

void WiFiMgr::begin() {
  auto cfg = Config::get();
  startAP(cfg);

  if (cfg.staSsid.length() > 0) {
    connectSTA(cfg.staSsid, cfg.staPass);
  } else {
    WiFi.mode(WIFI_AP); // STA 정보 없으면 AP only
  }
}

bool WiFiMgr::connectSTA(const String& ssid, const String& pass, uint32_t timeoutMs) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[STA] connecting to %s ...\n", ssid.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[STA] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("[STA] failed, keep AP");
    return false;
  }
}

String WiFiMgr::ip() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return WiFi.softAPIP().toString();
}
