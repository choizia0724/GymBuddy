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

  static const char* encToStr(wifi_auth_mode_t m) {
    switch (m) {
      case WIFI_AUTH_OPEN: return "OPEN";
      case WIFI_AUTH_WEP: return "WEP";
      case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
      case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
      case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
      case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENT";
      case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
      case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
      default: return "UNKNOWN";
    }
  }
  void setupWifiScanRoute() {
    // AP 페이지에서 스캔 가능하도록 AP+STA 모드 권장
    WiFi.mode(WIFI_AP_STA); // 이미 설정됐으면 중복 호출 무해

    server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* req){
      if (!authOK_(req)) return;

      // 스캔 실행 (비동기 원하면 true, 여기선 간단히 동기)
      int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);

      // 스트림 응답 시작
      AsyncResponseStream* resp = req->beginResponseStream("application/json");
      resp->print('[');

      if (n > 0) {
        bool first = true;
        for (int i = 0; i < n; ++i) {
          String ssid = WiFi.SSID(i);
          int32_t rssi = WiFi.RSSI(i);
          wifi_auth_mode_t enc = WiFi.encryptionType(i);
          String bssid = WiFi.BSSIDstr(i); // "aa:bb:cc:dd:ee:ff"

          // 숨김 SSID는 빈 문자열 → 프론트에서 제외하므로 그대로 내려도 OK
          // JSON 한 항목 직렬화
          StaticJsonDocument<256> doc;
          doc["ssid"]  = ssid;
          doc["rssi"]  = rssi;
          doc["enc"]   = encToStr(enc);
          doc["bssid"] = bssid;

          if (!first) resp->print(',');
          first = false;
          serializeJson(doc, *resp);
        }
        // 스캔 결과 캐시 지우기(선택)
        WiFi.scanDelete();
      }
      resp->print(']');
      req->send(resp);
    });
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

    server.on("/update", 
      HTTP_POST, 
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
          Serial.printf("FW OTA: %s\n", filename.c_str());
          // 펌웨어 파티션 (U_FLASH)로 시작
          if (!Update.begin(/*UPDATE_SIZE_UNKNOWN*/)) {
            Update.printError(Serial);
          }
        }
        if (Update.write(data, len) != (int)len) Update.printError(Serial);

        if (final) {
          if (!Update.end(true)) Update.printError(Serial);
          authed = false;
        }
      }
    );
    server.on("/fsupdate", 
      HTTP_POST,
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
          Serial.printf("FS OTA: %s\n", filename.c_str());
          // FS 파티션 대상으로 시작 (ESP32: U_SPIFFS 사용)
          // 크기를 모르면 UPDATE_SIZE_UNKNOWN로 시작 가능
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
          }
        }
        if (Update.write(data, len) != (int)len) Update.printError(Serial);

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
