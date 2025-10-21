#include "RestSender.h"
#include <WiFiClient.h>
#include <HTTPClient.h>

RestSender::RestSender(const Config& cfg) : cfg_(cfg) {
  if (cfg_.basePath == nullptr || cfg_.basePath[0] == '\0') cfg_.basePath = "/";
}

void RestSender::setAuthBearer(const String& token) { bearer_ = token; }
void RestSender::addHeader(const String& k, const String& v) { headers_.push_back({k, v}); }

String RestSender::buildUrl_(const String& path) const {
  String url;
  url += (cfg_.useHttps ? "https://" : "http://");
  url += cfg_.host;
  if ((cfg_.useHttps && cfg_.port != 443) || (!cfg_.useHttps && cfg_.port != 80)) {
    url += ":"; url += String(cfg_.port);
  }
  if (cfg_.basePath[0] != '/') url += "/";
  url += cfg_.basePath;
  if (!url.endsWith("/")) url += "/";
  if (path.length() && path[0] == '/') url += path.substring(1);
  else url += path;
  return url;
}


bool RestSender::post_plain_http(const char* host, uint16_t port,
                     const char* path, const String& json) {
  WiFiClient net;           // ★ 평문용
  HTTPClient http;

  if (!http.begin(net, host, port, path)) return false;

  http.setTimeout(4000);                     // 4초 타임아웃
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)json.c_str(), json.length());
  // String resp = http.getString();         // 필요하면 응답 읽기
  http.end();
  return (code >= 200 && code < 300);
}

