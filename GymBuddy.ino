#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "routes.h"
#include "nfc.h"
#include "laser.h"
#include "power.h"

const char* ssid = "ESP32-AP";
const char* password = "12345678";

WebServer server(8080);
bool isLoggedIn = false;

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point started at:");
  Serial.println(WiFi.softAPIP());

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  setupRoutes(server, isLoggedIn);
  server.begin();
}

void loop() {
  server.handleClient();
}
