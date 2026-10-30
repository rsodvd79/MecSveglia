#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

#define F(value) value
#define PROGMEM
template <typename T> inline T min(T a, T b) { return a < b ? a : b; }
template <typename T> inline T max(T a, T b) { return a > b ? a : b; }
using TickType_t = uint32_t;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
constexpr int pdTRUE = 1;
constexpr int portTICK_PERIOD_MS = 1;

extern unsigned long fakeMillis;
inline unsigned long millis() { return fakeMillis; }
inline void delay(unsigned long ms) { fakeMillis += ms; }
inline long random(long from, long) { return from; }
inline long map(long x, long a, long b, long c, long d) {
  return (x - a) * (d - c) / (b - a) + c;
}
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

struct FakeMutex { bool locked = false; };
using SemaphoreHandle_t = FakeMutex *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  static FakeMutex mutexes[16];
  static size_t count = 0;
  assert(count < 16);
  return &mutexes[count++];
}
inline int xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t) {
  if (mutex->locked) return 0;
  mutex->locked = true;
  return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex) {
  assert(mutex->locked);
  mutex->locked = false;
}

class String {
public:
  String() = default;
  String(const char *text) : value_(text ? text : "") {}
  String(const std::string &text) : value_(text) {}
  size_t length() const { return value_.length(); }
  bool isEmpty() const { return value_.empty(); }
  const char *c_str() const { return value_.c_str(); }
  char &operator[](size_t i) { return value_[i]; }
  char operator[](size_t i) const { return value_[i]; }
  bool concat(const String &other) {
    value_ += other.value_;
    return true;
  }
  String &operator+=(const String &other) {
    concat(other);
    return *this;
  }
  String &operator+=(char c) {
    value_ += c;
    return *this;
  }
  String operator+(const String &other) const {
    return String(value_ + other.value_);
  }
  bool operator==(const String &other) const { return value_ == other.value_; }
  bool operator!=(const String &other) const { return !(*this == other); }
  bool equalsIgnoreCase(const String &other) const {
    if (length() != other.length()) return false;
    for (size_t i = 0; i < length(); ++i) {
      if (std::tolower(static_cast<unsigned char>(value_[i])) !=
          std::tolower(static_cast<unsigned char>(other[i]))) return false;
    }
    return true;
  }
  void toUpperCase() {
    for (char &c : value_) c = std::toupper(static_cast<unsigned char>(c));
  }
  void replace(const String &from, const String &to) {
    size_t pos = 0;
    while ((pos = value_.find(from.value_, pos)) != std::string::npos) {
      value_.replace(pos, from.length(), to.value_);
      pos += to.length();
    }
  }
  void replace(char from, char to) {
    std::replace(value_.begin(), value_.end(), from, to);
  }
  int indexOf(char c, size_t start = 0) const {
    size_t pos = value_.find(c, start);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  int indexOf(const char *text) const {
    size_t pos = value_.find(text);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  void remove(size_t start, size_t count = std::string::npos) {
    value_.erase(start, count);
  }
  String substring(size_t start, size_t end) const {
    return String(value_.substr(start, end - start));
  }
  void trim() {
    const size_t start = value_.find_first_not_of(" \t\r\n");
    value_ = start == std::string::npos ? "" :
        value_.substr(start, value_.find_last_not_of(" \t\r\n") - start + 1);
  }

private:
  std::string value_;
};
