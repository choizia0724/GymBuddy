#include "routes.h"
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include "config.h"
#include "power.h"
#include "laser.h"

static bool ensureLoggedIn(WebServer& server, bool& isLoggedIn) {
  if (isLoggedIn) {
    return true;
  }

  server.send(401, "text/plain", "Unauthorized");
  return false;
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

  srv->on("/save", HTTP_POST, [srv, loginFlag]() {
    if (!*loginFlag) {
      srv->send(401, "text/plain", "Unauthorized");
      return;
    }

    auto current = Config::get();
    AppConfig updated = current;

    if (srv->hasArg("apSsid")) updated.apSsid = srv->arg("apSsid");
    if (srv->hasArg("apPass")) updated.apPass = srv->arg("apPass");
    if (srv->hasArg("staSsid")) updated.staSsid = srv->arg("staSsid");
    if (srv->hasArg("staPass")) updated.staPass = srv->arg("staPass");
    if (srv->hasArg("adminUser")) updated.adminUser = srv->arg("adminUser");
    if (srv->hasArg("adminPass")) updated.adminPass = srv->arg("adminPass");

    bool apChanged = (updated.apSsid != current.apSsid) || (updated.apPass != current.apPass);
    bool staChanged = (updated.staSsid != current.staSsid) || (updated.staPass != current.staPass);
    bool wantSTA = updated.staSsid.length() > 0;

    Config::save(updated);

    if (apChanged || staChanged) {
      WiFi.mode(wantSTA ? WIFI_AP_STA : WIFI_AP);
    }

    if (apChanged) {
      WiFi.softAPdisconnect(true);
      WiFi.softAP(updated.apSsid.c_str(), updated.apPass.c_str());
    }

    if (staChanged) {
      WiFi.disconnect(true);
      if (wantSTA) {
        WiFi.begin(updated.staSsid.c_str(), updated.staPass.c_str());
      }
    }

    redirectTo(*srv, "/config");
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
