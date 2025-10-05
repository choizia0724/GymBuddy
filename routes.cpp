#include "routes.h"
#include <LittleFS.h>
#include <Update.h>

void setupRoutes(WebServer& server, bool& isLoggedIn) {
  server.on("/", HTTP_GET, [&]() {
    if (!isLoggedIn) {
      File file = LittleFS.open("/login.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.sendHeader("Location", "/config", true);
      server.send(302, "text/plain", "");
    }
  });

  server.on("/login", HTTP_POST, [&]() {
    if (server.arg("user") == "admin" && server.arg("pass") == "esp32") {
      isLoggedIn = true;
      server.sendHeader("Location", "/config", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(401, "text/plain", "Unauthorized");
    }
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

  server.on("/update", HTTP_POST, []() {
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
