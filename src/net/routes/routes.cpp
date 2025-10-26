#include <ArduinoJson.h>
#include "routes.h"
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include "src/config/config.h"
#include "src/devices/power/power.h"
#include "src/devices/laser/laser.h"

static bool ensureLoggedIn(WebServer& server, bool& isLoggedIn) {
  if (isLoggedIn) {
    return true;
  }

  server.send(401, "text/plain", "Unauthorized");
  return false;
}

static void applyAndSaveConfig_(const AppConfig& in) {
  AppConfig cur = Config::get();

  bool apChanged  = (in.apSsid != cur.apSsid) || (in.apPass != cur.apPass);
  bool staChanged = (in.staSsid!= cur.staSsid)|| (in.staPass!= cur.staPass);
  bool wantSTA    = in.staSsid.length() > 0;

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

static void redirectTo(WebServer& server, const String& path) {
  server.sendHeader("Location", path, true);
  server.send(302, "text/plain", "");
}

static void redirectToLogin(WebServer& server) {
  redirectTo(server, "/");
}

void setupRoutes(WebServer& server, bool& isLoggedIn) {
  WebServer* srv = &server;
  bool* loginFlag = &isLoggedIn;

  srv->on("/", HTTP_GET, [srv, loginFlag]() {
    if (!*loginFlag) {
      File file = LittleFS.open("/login.html", "r");
      if (!file) {
        srv->send(500, "text/plain", "Missing login page");
        return;
      }
      srv->streamFile(file, "text/html");
      file.close();
    } else {
      redirectTo(*srv, "/config");
    }
  });

  srv->on("/login", HTTP_POST, [srv, loginFlag]() {
    auto cfg = Config::get();
    if (srv->arg("user") == cfg.adminUser && srv->arg("pass") == cfg.adminPass) {
      *loginFlag = true;
      srv->sendHeader("Location", "/config", true);
      srv->send(302, "text/plain", "");
    } else {
      srv->send(401, "text/plain", "Unauthorized");
    }
  });

  // GET: 현재 설정 조회
  srv->on("/api/config", HTTP_GET, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) return;
    AppConfig cfg = Config::get();
    StaticJsonDocument<512> doc;
    doc["apSsid"]    = cfg.apSsid;
    doc["apPass"]    = cfg.apPass;
    doc["staSsid"]   = cfg.staSsid;
    doc["staPass"]   = cfg.staPass;
    doc["adminUser"] = cfg.adminUser;
    doc["adminPass"] = cfg.adminPass;
    doc["version"]   = cfg.version;
    String out; serializeJson(doc, out);
    srv->send(200, "application/json", out);
  });

  // POST: 설정 저장(= 기존 /save 대체)
  srv->on("/api/config", HTTP_POST, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) return;
    if (!srv->hasArg("plain")) { srv->send(400, "text/plain", "no body"); return; }

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, srv->arg("plain"))) { srv->send(400, "text/plain", "bad json"); return; }

    AppConfig in = Config::get();
    if (doc.containsKey("apSsid"))    in.apSsid    = (const char*)doc["apSsid"];
    if (doc.containsKey("apPass"))    in.apPass    = (const char*)doc["apPass"];
    if (doc.containsKey("staSsid"))   in.staSsid   = (const char*)doc["staSsid"];
    if (doc.containsKey("staPass"))   in.staPass   = (const char*)doc["staPass"];
    if (doc.containsKey("adminUser")) in.adminUser = (const char*)doc["adminUser"];
    if (doc.containsKey("adminPass")) in.adminPass = (const char*)doc["adminPass"];
    if (doc.containsKey("version"))   in.version   = (uint32_t)doc["version"].as<uint32_t>();

    applyAndSaveConfig_(in);
    srv->send(204); // No Content
  });

  srv->on("/config", HTTP_GET, [srv, loginFlag]() {
    if (!*loginFlag) {
      redirectToLogin(*srv);
      return;
    }
    File file = LittleFS.open("/config.html", "r");
    if (!file) {
      srv->send(500, "text/plain", "Missing config page");
      return;
    }
    srv->streamFile(file, "text/html");
    file.close();
  });

  srv->on("/save", HTTP_ANY, [srv]() {
    srv->send(410, "text/plain", "Deprecated. Use POST /api/config (JSON).");
  });

  srv->on("/charger", HTTP_POST, [srv, loginFlag]() {
    if (!*loginFlag) {
      srv->sendHeader("Location", "/", true);
      srv->send(302, "text/plain", "");
      return;
    }

    if (!srv->hasArg("state")) {
      srv->send(400, "text/plain", "Missing 'state' parameter");
      return;
    }

    String state = srv->arg("state");
    bool   ok    = false;
    if (state == "on") {
      ok = Power::enableCharging();
    } else if (state == "off") {
      ok = Power::disableCharging();
    } else {
      srv->send(400, "text/plain", "Invalid state; use 'on' or 'off'");
      return;
    }

    if (!ok) {
      srv->send(500, "text/plain", "Failed to update charger state");
      return;
    }

    srv->send(200, "text/plain", Power::isChargingEnabled() ? "charging" : "idle");
  });

  srv->on("/laser/on", HTTP_POST, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) {
      return;
    }
    Laser::on();
    srv->send(200, "application/json", "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}");
  });

  srv->on("/laser/off", HTTP_POST, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) {
      return;
    }
    Laser::off();
    srv->send(200, "application/json", "{\"state\":\"off\"}");
  });

  srv->on("/laser/set", HTTP_POST, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) {
      return;
    }
    if (!srv->hasArg("freq") || !srv->hasArg("duty")) {
      srv->send(400, "text/plain", "Missing freq or duty");
      return;
    }

    long freq = srv->arg("freq").toInt();
    long duty = srv->arg("duty").toInt();

    if (freq < (long)Laser::MIN_FREQ_HZ || freq > (long)Laser::MAX_FREQ_HZ) {
      srv->send(400, "text/plain", "Frequency out of range");
      return;
    }
    if (duty < 0 || duty > 100) {
      srv->send(400, "text/plain", "Duty out of range");
      return;
    }

    Laser::setFreq(freq);
    Laser::setDuty(duty);
    Laser::on();
    srv->send(200, "application/json", "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}");
  });

  srv->on("/laser/status", HTTP_GET, [srv, loginFlag]() {
    if (!ensureLoggedIn(*srv, *loginFlag)) {
      return;
    }
    String payload = "{\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}";
    srv->send(200, "application/json", payload);
  });
  
  srv->on("/update", HTTP_GET, [srv, loginFlag]() {
    if (!*loginFlag) {
      redirectToLogin(*srv);
      return;
    }
    File file = LittleFS.open("/update.html", "r");
    if (!file) {
      srv->send(500, "text/plain", "Missing update page");
      return;
    }
    srv->streamFile(file, "text/html");
    file.close();
  });

  srv->on("/update", HTTP_POST, [srv]() {
    srv->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  }, [srv]() {
    HTTPUpload& upload = srv->upload();
    if (upload.status == UPLOAD_FILE_START) {
      Update.begin();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        srv->send(200, "text/plain", "Update Success! Rebooting...");
        delay(1000);
        ESP.restart();
      } else {
        srv->send(500, "text/plain", "Update Failed!");
      }
    }
  });
}
