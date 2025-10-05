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

static void redirectToLogin(WebServer& server) {
  server.sendHeader("Location", "/", true);
  redirectToLogin(server);
}

void setupRoutes(WebServer& server, bool& isLoggedIn) {
  server.on("/", HTTP_GET, [&]() {
    if (!isLoggedIn) {
      File file = LittleFS.open("/login.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.sendHeader("Location", "/config", true);
      redirectToLogin(server);
    }
  });

  server.on("/login", HTTP_POST, [&]() {
     auto cfg = Config::get();
    if (server.arg("user") == cfg.adminUser && server.arg("pass") == cfg.adminPass) {
      isLoggedIn = true;
      server.sendHeader("Location", "/config", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
  });

  server.on("/save", HTTP_POST, [&]() {
    if (!isLoggedIn) {
      server.send(401, "text/plain", "Unauthorized");
      return;
    }

    auto current = Config::get();
    AppConfig updated = current;

    if (server.hasArg("apSsid")) updated.apSsid = server.arg("apSsid");
    if (server.hasArg("apPass")) updated.apPass = server.arg("apPass");
    if (server.hasArg("staSsid")) updated.staSsid = server.arg("staSsid");
    if (server.hasArg("staPass")) updated.staPass = server.arg("staPass");
    if (server.hasArg("adminUser")) updated.adminUser = server.arg("adminUser");
    if (server.hasArg("adminPass")) updated.adminPass = server.arg("adminPass");

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

    server.sendHeader("Location", "/config", true);
    server.send(302, "text/plain", "");
  });

  server.on("/config", HTTP_GET, [&]() {
    if (!isLoggedIn) {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
    File file = LittleFS.open("/config.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/charger", HTTP_POST, [&]() {
    if (!isLoggedIn) {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }

    if (!server.hasArg("state")) {
      server.send(400, "text/plain", "Missing 'state' parameter");
      return;
    }

    String state = server.arg("state");
    bool   ok    = false;
    if (state == "on") {
      ok = Power::enableCharging();
    } else if (state == "off") {
      ok = Power::disableCharging();
    } else {
      server.send(400, "text/plain", "Invalid state; use 'on' or 'off'");
      return;
    }

    if (!ok) {
      server.send(500, "text/plain", "Failed to update charger state");
      return;
    }

    server.send(200, "text/plain", Power::isChargingEnabled() ? "charging" : "idle");
  });

  server.on("/laser/on", HTTP_POST, [&]() {
    if (!ensureLoggedIn(server, isLoggedIn)) {
      return;
    }
    Laser::on();
    server.send(200, "application/json", "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}");
  });

  server.on("/laser/off", HTTP_POST, [&]() {
    if (!ensureLoggedIn(server, isLoggedIn)) {
      return;
    }
    Laser::off();
    server.send(200, "application/json", "{\"state\":\"off\"}");
  });

  server.on("/laser/set", HTTP_POST, [&]() {
    if (!ensureLoggedIn(server, isLoggedIn)) {
      return;
    }
    if (!server.hasArg("freq") || !server.hasArg("duty")) {
      server.send(400, "text/plain", "Missing freq or duty");
      return;
    }

    long freq = server.arg("freq").toInt();
    long duty = server.arg("duty").toInt();

    if (freq < (long)Laser::MIN_FREQ_HZ || freq > (long)Laser::MAX_FREQ_HZ) {
      server.send(400, "text/plain", "Frequency out of range");
      return;
    }
    if (duty < 0 || duty > 100) {
      server.send(400, "text/plain", "Duty out of range");
      return;
    }

    Laser::setFreq(freq);
    Laser::setDuty(duty);
    Laser::on();
    server.send(200, "application/json", "{\"state\":\"on\",\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}");
  });

  server.on("/laser/status", HTTP_GET, [&]() {
    if (!ensureLoggedIn(server, isLoggedIn)) {
      return;
    }
    String payload = "{\"freq\":" + String(Laser::freq()) + ",\"duty\":" + String(Laser::duty()) + "}";
    server.send(200, "application/json", payload);
  });
  
  server.on("/update", HTTP_GET, [&]() {
    if (!isLoggedIn) {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
    File file = LittleFS.open("/update.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/update", HTTP_POST, [&]() {
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  }, [&]() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Update.begin();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        server.send(200, "text/plain", "Update Success! Rebooting...");
        delay(1000);
        ESP.restart();
      } else {
        server.send(500, "text/plain", "Update Failed!");
      }
    }
  });
}
