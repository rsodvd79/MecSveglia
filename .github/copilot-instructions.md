# MecSveglia – Copilot Instructions

ESP32-S3 smart clock/desk companion with a 128×32 OLED display, WiFi, NTP, OpenWeatherMap, ANSA RSS, web UI, mDNS, and OTA updates. Built with PlatformIO + Arduino framework.

## Build & Flash Commands

```bash
pio run                        # compile
pio run --target upload        # flash firmware
pio run --target uploadfs      # flash filesystem (LittleFS) — required when data/ changes
pio run --target clean         # clean build artifacts
pio device monitor             # serial monitor at 115200 baud
```

There are no unit tests in this repository.

## Architecture

Single main loop (`src/main.cpp`, ~1 750 lines) plus one FreeRTOS background task (`networkTask`) for network I/O. The task runs at 1 s ticks, updates RSS every hour and weather every 30 minutes — this prevents blocking the display render loop.

**Source modules:**

| File | Responsibility |
|---|---|
| `main.cpp` | Display management, all 8 screens, HTTP server, WiFi, OTA, timers |
| `eyes.cpp` | Eye animation state machine (wakeup → blink/saccade → sleep) |
| `rss.cpp` | RSS feed fetch & parse with mutex-protected cache |
| `meteo.cpp` | OpenWeatherMap fetch with mutex protection |
| `GOL.cpp` | Conway's Game of Life on a 64×32 cell grid mapped to 128×32 px |
| `screen.cpp` | Simple screen-index holder with 4 text rows |
| `Button.cpp` | Debounced button on GPIO12 (pull-up, active LOW) |

**Screens (index 0–7):**
`0` WiFi status · `1` Clock/date · `2` Eyes · `3` Weather · `4` Game of Life · `5` Triangle · `6` RSS · `7` Auto-cycle

## Key Conventions

### ArduinoJson version
This project uses ArduinoJson **v6** (`^6.21.2`). Use `StaticJsonDocument` / `DynamicJsonDocument`, not the v7 `JsonDocument` API.

### LittleFS alias
`platformio.ini` sets `board_build.filesystem = spiffs`, but `main.cpp` remaps it:
```cpp
#define LittleFS SPIFFS
```
Always use `LittleFS` in code; never call `SPIFFS` directly.

### Periodic timers
All timed events use a local `periodicMs` helper class (inside the `esp8266::polledTimeout` namespace, re-implemented to avoid ESP8266 dependency):
```cpp
static esp8266::polledTimeout::periodicMs showTimeNow(250);
if (showTimeNow) { /* runs every 250 ms */ }
```
Use this pattern for any new periodic work in the main loop.

### Thread-safe data access
`Meteo` and `Rss` classes own an internal `SemaphoreHandle_t _mutex`. Always call `lock()` / `unlock()` before reading or writing their shared data from outside the class. Do **not** access their internal fields directly from `networkTask` and the main loop simultaneously without the mutex.

In `rss.cpp`, HTTP fetch and XML stream parsing must happen **without** holding the mutex. Parse into local buffers first, then lock only for the brief copy into `_items[]`. Never hold the mutex across blocking network I/O or `delay()` calls.

In `meteo.cpp`, acquire the mutex only around the final assignment of parsed values to member variables, not around the HTTP request or JSON parsing. `classMeteo` fields (`temp`, `humidity`, `wind_speed`, `description`, `icon`, `snow`) are **public** — read them directly under the mutex, there are no getters.

### Forcing an immediate RSS refresh
To trigger an RSS update outside the normal 1-hour cycle, set the volatile flag from the main loop (or an HTTP handler):
```cpp
extern volatile bool forceRssRefresh;
forceRssRefresh = true;  // networkTask picks this up on its next tick
```

### WiFi reconnection
`networkTask` also handles WiFi recovery: it calls `WiFi.reconnect()` every 15 s while in STA mode, and retries switching from AP back to STA every 5 minutes when valid credentials exist. Do not add independent reconnect logic in `loop()`.

### AP fallback
When credentials are absent or the network is unreachable, `setup()` starts a **soft AP** with the mDNS name as SSID (default `MECSVEGLIA`). The device IP is shown on the display. mDNS is also started in AP mode so `MDNS.update()` is always safe to call.

### HTTP handlers
All routes are registered in `setup()` with `server.on(...)`. Handler lambdas directly call `server.send(...)`. The `__MDNS__` placeholder in `INDEX.HTM` is substituted at request time — keep this substitution when modifying the index handler.

### Display rendering
Each screen clears the buffer, draws to it, then calls `display.display()` **once**. Do not call `display.display()` twice in a row — it wastes ~8 ms per extra I2C write. Night dimming (22:00–06:59) uses `display.dim(true)`. Anti-burn-in inverts the display every 30 minutes via a periodic timer. When adding a new screen, follow the existing switch-case pattern in the main loop and assign it the next index.

### Timing and millis() rollover
All periodic checks in the main loop use the `periodicMs` helper (subtraction-based, rollover-safe). When writing timing logic **outside** that helper — e.g. scheduling the next animation frame — always use subtraction instead of direct comparison:
```cpp
// Corretto (gestisce il rollover dopo ~49 giorni):
if ((long)(millis() - nextUpdateMs) >= 0) { ... }

// Sbagliato:
if (millis() >= nextUpdateMs) { ... }
```

### Filesystem config files
All runtime configuration lives in `data/*.TXT` files loaded at boot. Modifications via HTTP POST handlers write back to LittleFS immediately. When adding a new config value, add a `.TXT` file in `data/`, load it in `setup()`, and expose it through the web interface.

### OTA
ArduinoOTA uses the mDNS name as its hostname. The display shows OTA progress %. The `networkTask` must tolerate OTA mid-flight (it runs at lower priority and only yields network calls).

## Hardware

- **MCU:** Waveshare ESP32-S3 Zero (`board/esp32-s3-fh4r2.json`) — 4 MB flash, 2 MB PSRAM
- **Display:** SSD1306 128×32, I2C, address `0x3C`, 3.3 V
- **Button:** GPIO 12, pull-up, active LOW
- **Build flags:** `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1` (USB-CDC serial)
- **Partition table:** `partitions.csv` (custom, required for LittleFS + OTA)
