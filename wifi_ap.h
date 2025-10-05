#pragma once
#include <Arduino.h>

namespace WiFiMgr {
  void begin();                    // AP 시작 + STA 연결 시도
  bool connectSTA(
    const String& ssid, 
    const String& pass, 
    uint32_t timeoutMs = 10000
    );
  String ip();                     // 현재 IP 문자열
}
