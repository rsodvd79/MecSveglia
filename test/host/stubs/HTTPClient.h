#pragma once

#include "Arduino.h"
#include <functional>

constexpr int HTTP_CODE_OK = 200;
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS = 1;

struct FakeResponse {
  std::string body;
  int code = HTTP_CODE_OK;
  std::string encoding;
  uint32_t byteInterval = 0;
  bool keepOpen = false;
  std::function<void()> duringGet;
};
extern FakeResponse fakeResponse;

class WiFiClient {
public:
  int available() {
    return position_ < fakeResponse.body.size() &&
        static_cast<uint32_t>(millis() - lastByte_) >= fakeResponse.byteInterval;
  }
  int read() {
    if (!available()) return -1;
    lastByte_ = millis();
    return static_cast<unsigned char>(fakeResponse.body[position_++]);
  }
  bool connected() {
    return position_ < fakeResponse.body.size() || fakeResponse.keepOpen;
  }
private:
  size_t position_ = 0;
  uint32_t lastByte_ = 0;
};

class HTTPClient {
public:
  bool begin(WiFiClient &, const String &) { return true; }
  void useHTTP10(bool) {}
  void setConnectTimeout(int) {}
  void setTimeout(int) {}
  void setFollowRedirects(int) {}
  void setRedirectLimit(int) {}
  void collectHeaders(const char **, size_t) {}
  int GET() {
    if (fakeResponse.duringGet) fakeResponse.duringGet();
    return fakeResponse.code;
  }
  String header(const char *) { return fakeResponse.encoding; }
  int getSize() { return fakeResponse.keepOpen ? -1 : fakeResponse.body.size(); }
  WiFiClient &getStream() { return stream_; }
  void end() {}
private:
  WiFiClient stream_;
};
