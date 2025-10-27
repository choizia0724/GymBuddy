// web.cpp (Unified - Basic Auth only)
#include "web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>

#include "src/config/config.h"
#include "src/net/wifi/wifi_ap.h"
#include "src/devices/power/power.h"
#include "src/devices/laser/laser.h"

// -----------------------------------------------------------------------------
// NOTE
// - 인증: HTTP Basic Auth (세션/쿠키/로그인 페이지 없음)
// - 보호 대상: 모든 민감 엔드포인트는 Basic Auth 강제
// - 정적 페이지: /admin.html, /config.html, /update.html (LittleFS)
// -----------------------------------------------------------------------------

namespace {
  AsyncWebServer server(80);

  inline bool authOK_(AsyncWebServerRequest* req) {
    const auto& cfg = Config::get();
    if (!req->authenticate(cfg.adminUser.c_str(), cfg.adminPass.c_str())) {
      req->requestAuthentication();   // 401 + WWW-Authenticate
      return false;
    }
    return true;
  }

  void applyAndSaveConfig_(const AppConfig& in) {
    AppConfig cur = Config::get();

    const bool apChanged  = (in.apSsid != cur.apSsid) || (in.apPass != cur.apPass);
    const bool staChanged = (in.staSsid!= cur.staSsid)|| (in.staPass!= cur.staPass);
    const bool wantSTA    = in.staSsid.length() > 0;

    Config::save(in);

    if (apChanged || staChanged) {
      WiFi.mode(wantSTA ? WIFI_AP_STA : WIFI_AP);
    }
    if (apChanged) {
      WiFi.softAPdisconnect(true);
      WiFi.softAP(in.apSsid.c_str(), in.apPass.c_str());
    }
    if (staChanged) {
      WiFi.disconnect(true);
      if (wantSTA) {
        WiFi.begin(in.staSsid.c_str(), in.staPass.c_str());
      }
    }
  }

  // ---------- API: Config ----------
  void handleGetConfig(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;

    StaticJsonDocument<512> doc;
    const auto cfg = Config::get();

    doc["apSsid"]    = cfg.apSsid;
    doc["apPass"]    = cfg.apPass;
    doc["staSsid"]   = cfg.staSsid;
    doc["staPass"]   = cfg.staPass;
    doc["adminUser"] = cfg.adminUser;
    doc["adminPass"] = cfg.adminPass;
    doc["version"]   = cfg.version;

    String json; serializeJson(doc, json);
    req->send(200, "application/json", json);
  }

  void handlePostConfigBody(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    if (!authOK_(req)) return;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, data, len)) {
      req->send(400, "text/plain", "Invalid JSON");
      return;
    }

    AppConfig in = Config::get();
    if (doc.containsKey("apSsid"))    in.apSsid    = (const char*)doc["apSsid"];
    if (doc.containsKey("apPass"))    in.apPass    = (const char*)doc["apPass"];
    if (doc.containsKey("staSsid"))   in.staSsid   = (const char*)doc["staSsid"];
    if (doc.containsKey("staPass"))   in.staPass   = (const char*)doc["staPass"];
    if (doc.containsKey("adminUser")) in.adminUser = (const char*)doc["adminUser"];
    if (doc.containsKey("adminPass")) in.adminPass = (const char*)doc["adminPass"];
    if (doc.containsKey("version"))   in.version   = doc["version"].as<uint32_t>();

    applyAndSaveConfig_(in);
    req->send(204); // No Content
  }

  // ---------- Auth check ----------
  void handleAuthCheck(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;
    req->send(200, "application/json", "{\"ok\":true}");
  }

  // ---------- System ----------
  void handleStatus(AsyncWebServerRequest* req) {
    String body = "IP=" + WiFiMgr::ip();
    req->send(200, "text/plain", body);
  }

  void handleReboot(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;
    req->send(200, "text/plain", "Rebooting...");
    delay(200);
    ESP.restart();
  }

  // ---------- Charger ----------
  void handleCharger(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;
    if (!req->hasParam("state", true)) { req->send(400, "text/plain", "Missing 'state'"); return; }

    String state = req->getParam("state", true)->value();
    bool ok = false;
    if (state == "on")      ok = Power::enableCharging();
    else if (state == "off") ok = Power::disableCharging();
    else { req->send(400, "text/plain", "Invalid state; use 'on' or 'off'"); return; }

    if (!ok) { req->send(500, "text/plain", "Failed to update"); return; }
    req->send(200, "text/plain", Power::isChargingEnabled() ? "charging" : "idle");
  }

  // ---------- Laser ----------
  void handleLaserOn(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;
    Laser::on();
    String json = "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}";
    req->send(200, "application/json", json);
  }

  void handleLaserOff(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;
    Laser::off();
    req->send(200, "application/json", "{\"state\":\"off\"}");
  }

  void handleLaserSet(AsyncWebServerRequest* req) {
    if (!authOK_(req)) return;

    if (!req->hasParam("freq", true) || !req->hasParam("duty", true)) {
      req->send(400, "text/plain", "Missing freq or duty");
      return;
    }
    long freq = req->getParam("freq", true)->value().toInt();
    long duty = req->getParam("duty", true)->value().toInt();

    if (freq < (long)Laser::MIN_FREQ_HZ || freq > (long)Laser::MAX_FREQ_HZ) {
      req->send(400, "text/plain", "Frequency out of range"); return;
    }
    if (duty < 0 || duty > 100) {
      req->send(400, "text/plain", "Duty out of range"); return;
    }

    Laser::setFreq(freq);
    Laser::setDuty(duty);
    Laser::on();
    String json = "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}";
    req->send(200, "application/json", json);
  }

  // ---------- OTA (/update) ----------
  void registerHttpOta() {
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req){
      if (!authOK_(req)) return;
      if (LittleFS.exists("/update.html")) {
        req->send(LittleFS, "/update.html", "text/html");
      } else {
        req->send(200, "text/html",
          "<form method='POST' action='/update' enctype='multipart/form-data'>"
          "<input type='file' name='firmware'> <input type='submit' value='Update'>"
          "</form>");
      }
    });

    server.on("/update", HTTP_POST,
      [](AsyncWebServerRequest* req){
        if (!authOK_(req)) return;
        const bool ok = !Update.hasError();
        req->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
        if (ok) { delay(300); ESP.restart(); }
      },
      [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        static bool authed = false;
        if (!authed) { if (!authOK_(req)) return; authed = true; }

        if (!index) {
          Serial.printf("OTA: %s\n", filename.c_str());
          if (!Update.begin()) Update.printError(Serial);
        }
        if (Update.write(data, len) != (int)len) {
          Update.printError(Serial);
        }
        if (final) {
          if (!Update.end(true)) Update.printError(Serial);
          authed = false;
        }
      }
    );
  }
} // namespace

void WebServerApp::begin() {
  LittleFS.begin(true);

  // ---------- Static pages (protected) ----------
  // 루트: admin.html 있으면 인증 후 서빙, 없으면 상태 문자열
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    if (!authOK_(req)) return;
    if (LittleFS.exists("/config.html")) {
      req->send(LittleFS, "/config.html", "text/html");
    } else {
      req->send(200, "text/plain", "Admin page not found (LittleFS /config.html).");
    }
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req){
    if (!authOK_(req)) return;
    if (LittleFS.exists("/config.html")) {
      req->send(LittleFS, "/config.html", "text/html");
    } else {
      req->send(404, "text/plain", "Missing /config.html");
    }
  });

  // ---------- API ----------
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest* req){ /* header ack; body in next lambda */ },
    nullptr,
    handlePostConfigBody
  );

  server.on("/auth/check", HTTP_GET, handleAuthCheck);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reboot", HTTP_POST, handleReboot);

  // Charger / Laser
  server.on("/charger", HTTP_POST, handleCharger);
  server.on("/laser/on", HTTP_POST, handleLaserOn);
  server.on("/laser/off", HTTP_POST, handleLaserOff);
  server.on("/laser/set", HTTP_POST, handleLaserSet);

  // OTA
  registerHttpOta();

  // Deprecated endpoint
  server.on("/save", HTTP_ANY, [](AsyncWebServerRequest* req){
    req->send(410, "text/plain", "Deprecated. Use POST /api/config (JSON).");
  });

  server.begin();
  Serial.println(String("[WEB] server started at http://") + WiFiMgr::ip());
}
