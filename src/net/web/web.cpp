#include "web.h"
#include "src/config/config.h"
#include "src/net/wifi/wifi_ap.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>   // HTTP OTA용

namespace {
  AsyncWebServer server(80);

  bool basicAuthOK(AsyncWebServerRequest* req, const AppConfig& cfg) {
    if (!req->authenticate(cfg.adminUser.c_str(), cfg.adminPass.c_str())) {
      req->requestAuthentication();
      return false;
    }
    return true;
  }

  void handleGetConfig(AsyncWebServerRequest* req) {
    if (!basicAuthOK(req, Config::get())) return;
    StaticJsonDocument<512> doc;
    auto cfg = Config::get();
    doc["apSsid"] = cfg.apSsid;
    doc["apPass"] = cfg.apPass;
    doc["staSsid"] = cfg.staSsid;
    doc["staPass"] = cfg.staPass;
    doc["adminUser"] = cfg.adminUser;
    doc["adminPass"] = cfg.adminPass;
    doc["version"]   = cfg.version;
    String json; serializeJson(doc, json);
    req->send(200, "application/json", json);
  }

  void handlePostConfig(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t idx, size_t total) {
    // JSON Body 처리
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { req->send(400, "text/plain", "Invalid JSON"); return; }

    AppConfig cfg = Config::get();
    if (doc.containsKey("apSsid"))    cfg.apSsid = (const char*)doc["apSsid"];
    if (doc.containsKey("apPass"))    cfg.apPass = (const char*)doc["apPass"];
    if (doc.containsKey("staSsid"))   cfg.staSsid= (const char*)doc["staSsid"];
    if (doc.containsKey("staPass"))   cfg.staPass= (const char*)doc["staPass"];
    if (doc.containsKey("adminUser")) cfg.adminUser=(const char*)doc["adminUser"];
    if (doc.containsKey("adminPass")) cfg.adminPass=(const char*)doc["adminPass"];
    if (doc.containsKey("version"))   cfg.version = doc["version"].as<uint32_t>();
    Config::save(cfg);

    req->send(200, "application/json", "{\"ok\":true}");
  }

  void handleReboot(AsyncWebServerRequest* req) {
    if (!basicAuthOK(req, Config::get())) return;
    req->send(200, "text/plain", "Rebooting...");
    delay(200);
    ESP.restart();
  }

  // HTTP OTA (파일 업로드로 .bin 갱신)
  void registerHttpOta() {
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req){
      if (!basicAuthOK(req, Config::get())) return;
      req->send(200, "text/html",
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='firmware'> <input type='submit' value='Update'>"
        "</form>");
    });

    server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest* req){
        if (!basicAuthOK(req, Config::get())) return;
        bool ok = !Update.hasError();
        req->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
        delay(200);
        if (ok) ESP.restart();
      },
      [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        static bool authChecked = false;
        if (!authChecked) { if (!basicAuthOK(req, Config::get())) return; authChecked = true; }
        if (!index) {
          Serial.printf("OTA: %s\n", filename.c_str());
          if (!Update.begin()) Update.printError(Serial);
        }
        if (Update.write(data, len) != len) Update.printError(Serial);
        if (final) { if (!Update.end(true)) Update.printError(Serial); authChecked = false; }
      }
    );
  }
}

void WebServerApp::begin() {
  LittleFS.begin(true);

  // 정적 파일 (admin.html)
  server.serveStatic("/", LittleFS, "/admin.html").setDefaultFile("admin.html");

  // API
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req){ /* body handler */ },
            NULL,
            handlePostConfig);

  server.on("/reboot", HTTP_POST, handleReboot);

  registerHttpOta();

  // 상태 확인
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req){
    String s = "IP=" + WiFiMgr::ip();
    req->send(200, "text/plain", s);
  });

  server.begin();
  Serial.println("[WEB] server started at http://"+ WiFiMgr::ip());
}
