#include "Arduino.h"
#include "WiFi.h"
#include "wifi_credentials.h"
#include "esp_task_wdt.h"
#include <atomic>
#include <ctime>
#include <iostream>
#include <map>

unsigned long fakeMillis = 0;
String ssid = "existing", password = " old key ";
static std::atomic<bool> timeSyncCompleted{false};
static bool timezoneConfigured = true, timeSyncedOnce = false;
static time_t lastSuccessfulSync = 0;
static uint32_t lastTimeSyncAttemptMs = 0;
static const time_t RESYNC_INTERVAL_SEC = 12 * 60 * 60;
static const uint32_t SYNC_RETRY_INTERVAL_MS = 60000;
static time_t currentTime = 1700000000;
static unsigned int syncAttempts = 0;
static bool timeIsSane(time_t value) { return value >= 1672531200; }
static void startTimeSync() {
  ++syncAttempts;
  lastTimeSyncAttemptMs = millis();
}
static time_t fakeTime(time_t *) { return currentTime; }

struct FakeServer {
  std::map<std::string, String> args;
  int code = 0;
  bool hasArg(const char *name) { return args.count(name) != 0; }
  String arg(const char *name) { return args.at(name); }
  void send(int status, const String &, const String &) { code = status; }
} server;

struct File {
  std::string *data;
  explicit operator bool() const { return data != nullptr; }
  size_t print(const String &value) {
    *data = value.c_str();
    return value.length();
  }
  void close() {}
};
struct FakeFS {
  std::map<std::string, std::string> files;
  bool mountOk = true, mounted = true, formatted = false;
  unsigned int mounts = 0;
  bool begin(bool format = false) {
    ++mounts;
    formatted = formatted || format;
    mounted = mountOk;
    return mounted;
  }
  void end() { mounted = false; }
  bool exists(const char *name) { return files.count(name) != 0; }
  File open(const char *name, const char *) {
    if (!mounted) return File{nullptr};
    files[name].clear();
    return File{&files[name]};
  }
} LittleFS;
struct FakeESP {
  bool restarted = false;
  void restart() { restarted = true; }
} ESP;
struct FakeDisplay {
  void setTextSize(int) {}
  void clearDisplay() {}
  void setCursor(int, int) {}
  template <typename T> void println(T) {}
  template <typename T> void print(T) {}
  void display() {}
} display;
struct FakeSerial {
  bool logged = false;
  void println(const char *) { logged = true; }
} Serial;
constexpr int LED_BUILTIN = 1, LOW = 0, HIGH = 1, U_SPIFFS = 100;
static void digitalWrite(int, int) {}
enum ota_error_t {
  OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR,
  OTA_RECEIVE_ERROR, OTA_END_ERROR
};
struct FakeOTA {
  int command = U_SPIFFS;
  int getCommand() { return command; }
} ArduinoOTA;
static std::atomic<bool> otaInProgress{false};
static bool otaFilesystemUnmounted = false;

#define time fakeTime
#include "handlers.inc"
#undef time

int main() {
  LittleFS.files["/WIFI_PWD.TXT"] = password.c_str();
  server.args = {{"ssid", std::string(32, 's')}, {"pwd", " new key "}};
  handleSetupPost();
  assert(server.code == 200 && ESP.restarted);
  assert(LittleFS.files["/WIFI_SID.TXT"] == std::string(32, 's'));
  assert(LittleFS.files["/WIFI_PWD.TXT"] == " new key ");
  ESP.restarted = false;
  server.args = {{"ssid", std::string(33, 's')}, {"pwd", "password"}};
  handleSetupPost();
  assert(server.code == 400 && !ESP.restarted);
  assert(LittleFS.files["/WIFI_PWD.TXT"] == " new key ");
  server.args = {{"ssid", "open"}, {"pwd", ""}};
  handleSetupPost();
  assert(server.code == 200 && LittleFS.files["/WIFI_PWD.TXT"].empty());
  LittleFS.files["/WIFI_PWD.TXT"] = password.c_str();
  server.args = {{"ssid", " spaces "}};
  handleSetupPost();
  assert(server.code == 200);
  assert(LittleFS.files["/WIFI_SID.TXT"] == " spaces ");
  assert(LittleFS.files["/WIFI_PWD.TXT"] == password.c_str());
  server.args = {{"ssid", "valid"}, {"pwd", "short"}};
  handleSetupPost();
  assert(server.code == 400);
  assert(LittleFS.files["/WIFI_SID.TXT"] == " spaces ");
  LittleFS.files.erase("/WIFI_PWD.TXT");
  server.args = {{"ssid", "requires-explicit-password"}};
  handleSetupPost();
  assert(server.code == 400 && !LittleFS.exists("/WIFI_PWD.TXT"));
  assert(LittleFS.files["/WIFI_SID.TXT"] == " spaces ");
  for (int rssi = -120; rssi <= 10; ++rssi) {
    int quality = dBmtoPercentage(rssi);
    assert(quality >= 0 && quality <= 100);
  }
  assert(dBmtoPercentage(-100) == 0 && dBmtoPercentage(-75) == 50);
  assert(dBmtoPercentage(-50) == 100 && dBmtoPercentage(-40) == 100);

  lastSuccessfulSync = 17;
  time_is_set_scheduled();
  assert(timeSyncCompleted && !timeSyncedOnce && lastSuccessfulSync == 17);
  maintainTimeSync();
  assert(!timeSyncCompleted && timeSyncedOnce && lastSuccessfulSync == currentTime);
  ++currentTime;
  maintainTimeSync();
  assert(lastSuccessfulSync == currentTime - 1 && syncAttempts == 0);

  simulateOtaStart();
  assert(otaInProgress && otaFilesystemUnmounted && !LittleFS.mounted);
  unsigned int mounts = LittleFS.mounts;
  simulateOtaError(OTA_RECEIVE_ERROR);
  assert(!otaInProgress && !otaFilesystemUnmounted && LittleFS.mounted);
  assert(LittleFS.mounts == mounts + 1 && !LittleFS.formatted);
  simulateOtaError(OTA_AUTH_ERROR);
  assert(LittleFS.mounts == mounts + 1);
  simulateOtaStart();
  LittleFS.mountOk = false;
  simulateOtaError(OTA_RECEIVE_ERROR);
  assert(!LittleFS.mounted && Serial.logged && !LittleFS.formatted);
  simulateOtaEnd();
  assert(!otaInProgress && !otaFilesystemUnmounted);
  for (unsigned int step = 0; step <= 40; ++step) {
    fakeMillis += 1000;
    simulateOtaProgress(step * 25, 1000);
    assert(millis() - lastWatchdogReset() < 30000);
  }
  std::cout << "HTTP/SNTP/OTA handler regressions passed\n";
}
