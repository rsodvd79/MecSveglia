# MecSveglia

ESP32-S3 smart clock/desk companion (Waveshare Zero). PlatformIO + Arduino.

## Commands

| Azione | Comando |
|---|---|
| Compila | `pio run` |
| Flash firmware | `pio run --target upload` |
| Flash filesystem | `pio run --target uploadfs` |
| Monitor seriale | `pio device monitor` (115200 baud) |

**Nessun test presente** nel repository.

## LittleFS

`platformio.ini` dichiara `board_build.filesystem = spiffs` ma il codice mappa:
```cpp
#define LittleFS SPIFFS
```
Usa sempre `LittleFS` nel codice, mai `SPIFFS` direttamente.

## Architettura

`src/main.cpp` (~1820 righe) contiene: loop principale, HTTP server, WiFi, OTA, timers, tutti gli 8 screen. Un task FreeRTOS (`networkTask`) gestisce RSS/meteo/scansione WiFi — priority 1, stack 8KB, tick 1s.

Moduli sorgente:

| File | Ruolo |
|---|---|
| `main.cpp` | Display, HTTP, WiFi, OTA, timers, tutti gli screen |
| `eyes.cpp` | Macchina a stati animazione occhi (wakeup→blink/saccade→sleep) |
| `rss.cpp` | Fetch/parse feed RSS con cache protetta da mutex |
| `meteo.cpp` | Fetch OpenWeatherMap con mutex |
| `GOL.cpp` | Game of Life 64×32, rilevamento stasi + iniezione blinker |
| `screen.cpp` | Indice schermo corrente, 4 righe di testo per boot log |
| `Button.cpp` | Debounce (250ms) su GPIO12, INPUT_PULLUP, attivo LOW |

## Schermi (0-7)

0: WiFi · 1: Ora/data · 2: Occhi · 3: Meteo · 4: Game of Life · 5: Triangolo · 6: RSS · 7: Ciclo automatico

Screen 7 mostra testo "CICLO" rimbalzante quando non in ciclo; se `CicloScreen=true` avanza tra 1-6 ogni ~60s.

## Pattern temporali

Tutti i timer usano `esp8266::polledTimeout::periodicMs` (implementato in main.cpp):
```cpp
static esp8266::polledTimeout::periodicMs showTimeNow(250);
if (showTimeNow) { /* ogni 250ms */ }
```
Per logica temporale fuori dal helper, usa sempre sottrazione: `if ((long)(millis() - t0) >= 0)`.

## Mutex per dati condivisi

`classMeteo` e `classRss` hanno `_mutex` interno. Chiama `lock()`/`unlock()` prima di leggere/scrivere campi pubblici da contesti diversi (main loop vs networkTask).

**RSS**: fetch HTTP e parsing XML senza mutex; lock solo per la copia breve in `_items[]`.

**Meteo**: lock solo nell'assegnazione finale dei campi (`temp`, `humidity`, `wind_speed`, `description`, `icon`, `snow`). Sono tutti `public`.

## WiFi

- STA se `WIFI_SID.TXT` + `WIFI_PWD.TXT` su LittleFS, altrimenti AP con SSID = mDNS name
- `networkTask` gestisce auto-reconnect (15s) e ritorno AP→STA (5min). Non aggiungere logica in `loop()`.
- Fallback AP: WiFi.persistent(false), TX power limitato a 11dBm
- mDNS avviato anche in AP mode

## HTTP

Route principali: `/`, `/setup`, `/status`, `/config`, `/wifi`, `/time`, `/eyes`, `/meteo`, `/tri`, `/gol`, `/golreset`, `/cycle`, `/rssscreen`, `/rss`, `/oled`, `/timezone`, `/screen/start`, `/favicon.ico`

`handleRoot()` serve `INDEX.HTM` rimpiazzando `__MDNS__` col nome mDNS a runtime. `M404.HTM` opzionale su LittleFS per 404 custom.

## Config su filesystem

Tutti i file `.TXT` in `data/` letti in `setup()`. Scrittura immediata su HTTP POST.

| File | Contenuto |
|---|---|
| `WIFI_SID.TXT` | SSID WiFi |
| `WIFI_PWD.TXT` | Password WiFi (in `.gitignore`) |
| `MDNS_NM.TXT` | Nome mDNS (default MECSVEGLIA) |
| `TIME_ZONE.TXT` | POSIX TZ string (default `CET-1CEST,M3.5.0/2,M10.5.0/3`) |
| `SCREEN.TXT` | Schermo iniziale 0-7 |
| `METEO_API.TXT` | Query string OpenWeatherMap dopo `?` |
| `RSS.TXT` | Categoria RSS ANSA |

## ArduinoJson

**Usa v6** (`StaticJsonDocument`/`DynamicJsonDocument`). La libreria in `platformio.ini` è `^6.21.2`.

## OTA

Hostname = mDNS name. Display mostra progress %. WDT 30s abilitato.

## Display

- SSD1306 128×32, I2C addr 0x3C
- `display.dim(true)` in notturna (22:00-06:59)
- Ogni screen: clear → draw → `display.display()` **una volta**
- Inversione anti burn-in ogni 30 min (disabilitata nel codice ma timer presente)

## Board custom

Definizione in `board/esp32-s3-fh4r2.json` (Waveshare ESP32-S3 Zero, 4MB flash + 2MB PSRAM). Build flags: `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`.

## Partitioni (`partitions.csv`)

`app0` (0x10000, ~2MB) + `spiffs` (0x200000, 2MB). Supporta OTA (due slot app).

## Risorse informative esistenti

- `.github/copilot-instructions.md` — documentazione complementare dettagliata
- `info/esp32-S3-Zero-Waveshare-pinout-low.jpg` — pinout
- `README.md` — panoramica estesa

## Da sapere

- `screen` va in ricorsione: `Next()` incrementa e resetta a 0 se > max
- `GOL.Update()` rileva stasi (hash FNV-1a) e inietta blinker automaticamente
- `networkTask` forza refresh RSS e meteo alla prima connessione WiFi (flag atomici `forceRssRefresh`, `forceMeteoRefresh`)
- `needsMdnsRestart` atomico per riavviare mDNS dopo riconnessione AP→STA
- Il pulsante breve fa lampeggiare il display (200ms invert) poi cambia screen
- `showScreenInit(screen.Current(s))` è il modo corretto per cambiare screen da HTTP handler
- LED_BUILTIN usato come indicatore attività (LOW = busy, HIGH = idle)
