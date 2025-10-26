#pragma once
#include <Arduino.h>
#include <vector>

class RestSender {
public:
  struct Config {
    const char* host        = "";     // 예: "api.example.com"
    uint16_t    port        = 80;    // 443=HTTPS, 80=HTTP
    const char* basePath    = "/";    // 예: "/prod"
    bool        useHttps    = false;   // true=HTTPS, false=HTTP
    uint16_t    timeoutMs   = 4000;   // 요청 타임아웃
    uint8_t     maxRetries  = 2;      // 재시도 횟수
  };

  explicit RestSender(const Config& cfg);

  void setAuthBearer(const String& token);                // Authorization: Bearer <token>
  void addHeader(const String& key, const String& value); // 커스텀 헤더

  bool post_plain_http(const char* host, uint16_t port, const char* path, const String& json);

private:
  String buildUrl_(const String& path) const;

  Config cfg_;
  String bearer_;
  std::vector<std::pair<String,String>> headers_;
};
