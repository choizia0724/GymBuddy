#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
  // AP
  String apSsid     = "GymBuddy-AP";
  String apPass     = "11111111";   // 8자 이상
  // STA
  String staSsid    = "";         
  String staPass    = "";
  // Admin
  String adminUser  = "admin";
  String adminPass  = "admin";
  // 버전/기타
  uint32_t version  = 0.1;
};

namespace Config {
  void begin();
  AppConfig get();
  void save(const AppConfig& cfg);
}
