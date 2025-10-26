#include "config.h"

namespace {
  Preferences prefs;
  AppConfig cached;
  const char* NS = "cfg";
}

void Config::begin() {
  prefs.begin(NS, false);
  // 존재하면 로드
  cached.apSsid  = prefs.getString("apSsid",  cached.apSsid);
  cached.apPass  = prefs.getString("apPass",  cached.apPass);
  cached.staSsid = prefs.getString("staSsid", cached.staSsid);
  cached.staPass = prefs.getString("staPass", cached.staPass);
  cached.adminUser = prefs.getString("admU",  cached.adminUser);
  cached.adminPass = prefs.getString("admP",  cached.adminPass);
  cached.version = prefs.getULong("ver", cached.version);
}

AppConfig Config::get() { return cached; }

void Config::save(const AppConfig& cfg) {
  cached = cfg;
  prefs.putString("apSsid",  cached.apSsid);
  prefs.putString("apPass",  cached.apPass);
  prefs.putString("staSsid", cached.staSsid);
  prefs.putString("staPass", cached.staPass);
  prefs.putString("admU",    cached.adminUser);
  prefs.putString("admP",    cached.adminPass);
  prefs.putULong ("ver",     cached.version);
}
