#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
  // AP
  String apSsid     = "GymBuddy-AP";
  String apPass     = "11111111";   // 8자 이상
  // STA
  String staSsid    = "SK_WiFiGIGAA154_2.4G";         
  String staPass    = "2004063771";
  // Admin
  String adminUser  = "admin";
  String adminPass  = "admin";
  // Server
  String serverUrl  = "localhost";
  String port       = "8080";
  // 버전/기타
  uint32_t version  = 0.1;
  String deviceId   = "GymBuddy-Yeongdeungpo-01";
};

namespace Config {
  void begin();
  AppConfig get();
  void save(const AppConfig& cfg);
}
