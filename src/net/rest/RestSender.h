#pragma once
#include <Arduino.h>
#include <vector>

class RestSender {
public:
  struct Config {
    const char* host        = "isluel.iptime.org";     // 예: "api.example.com"
    uint16_t    port        = 35184;     // 443=HTTPS, 80=HTTP
    const char* basePath    = "/api/v2/esp/count";    // 예: "/prod"
    bool        useHttps    = false;  // true=HTTPS, false=HTTP
    uint16_t    timeoutMs   = 4000;   // 요청 타임아웃
    uint8_t     maxRetries  = 2;      // 재시도 횟수
  };

  explicit RestSender(const Config& cfg);

  void setAuthBearer(const String& token);                // Authorization: Bearer <token>
  void addHeader(const String& key, const String& value); // 커스텀 헤더

  bool post_plain_http(const String& json);

private:
  String buildUrl_(const String& path) const;

  Config cfg_;
  String bearer_;
  std::vector<std::pair<String,String>> headers_;
};
