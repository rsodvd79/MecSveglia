#pragma once

#include <HTTPClient.h>
#include <esp_task_wdt.h>

inline void configureFeedRequest(HTTPClient &http) {
  http.useHTTP10(true); // getStream() does not decode HTTP/1.1 chunk framing.
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(1);
  const char *headers[] = {"Transfer-Encoding"};
  http.collectHeaders(headers, 1);
}

inline bool feedResponseIsReadable(HTTPClient &http) {
  String encoding = http.header("Transfer-Encoding");
  return encoding.isEmpty() || encoding.equalsIgnoreCase("identity");
}

// Both parsers use an absolute body deadline, not a timeout renewed per byte.
class FeedReader {
public:
  explicit FeedReader(HTTPClient &http)
      : stream_(http.getStream()), remaining_(http.getSize()),
        started_(millis()), lastByte_(started_), lastYield_(started_) {
    esp_task_wdt_reset();
  }

  int read() {
    while (remaining_ != 0) {
      const uint32_t now = millis();
      if (now - started_ >= 15000UL || now - lastByte_ >= 5000UL) {
        return -1;
      }
      if (now - lastYield_ >= 10UL) {
        esp_task_wdt_reset();
        delay(1);
        lastYield_ = now;
      }
      if (stream_.available()) {
        int c = stream_.read();
        if (c >= 0) {
          lastByte_ = now;
          if (remaining_ > 0) {
            --remaining_;
          }
          return c;
        }
      } else if (!stream_.connected()) {
        return -1;
      } else {
        delay(1);
      }
    }
    return -1;
  }

  size_t readBytes(char *buffer, size_t length) {
    size_t count = 0;
    for (; count < length; ++count) {
      int c = read();
      if (c < 0) {
        break;
      }
      buffer[count] = static_cast<char>(c);
    }
    return count;
  }

private:
  WiFiClient &stream_;
  int remaining_;
  uint32_t started_;
  uint32_t lastByte_;
  uint32_t lastYield_;
};
