MecSveglia — ESP32-S3 Zero clock/desk companion

- Piattaforma: ESP32‑S3 (Waveshare ESP32‑S3‑Zero)
- Display: OLED SSD1306 128×32 I2C (addr 0x3C)
- Funzioni: ora via NTP (Europe/Rome), meteo OpenWeatherMap, notizie RSS ANSA, pagine web su LittleFS, mDNS, OTA, animazioni (occhi, Game of Life, triangolo), ciclo schermi automatico, attenuazione notturna.

Pinout della board: vedi immagine in `info/esp32-S3-Zero-Waveshare-pinout-low.jpg`.

![Pinout Waveshare ESP32‑S3‑Zero](info/esp32-S3-Zero-Waveshare-pinout-low.jpg)

Requisiti

- VS Code + PlatformIO (framework Arduino)
- Librerie auto‑gestite da PlatformIO: `Adafruit SSD1306`, `Adafruit GFX`, `ArduinoJson`, `Adafruit NeoPixel`

Struttura progetto

- Codice sorgente: `src/`
- File web e configurazione su filesystem: `data/` (caricati su LittleFS)
- Definizioni scheda e pinout: `info/`
- Modelli per stampa 3D: `stl/` (es. `stl/mec01.stl`, `stl/mac02.stl`)

Novità Versione Corrente

- **Multitasking FreeRTOS**: Le operazioni di rete (RSS, Meteo) sono gestite in un task separato per evitare blocchi del display.
- **LittleFS**: Migrazione da SPIFFS a LittleFS per una gestione file più efficiente e moderna.
- **RSS Streaming**: Parsing ottimizzato dei feed RSS per ridurre l'uso della memoria e prevenire frammentazione.
- **Thread Safety**: Accesso ai dati condivisi protetto da mutex.
- **Game of Life migliorato**: Inizializzazione casuale ad ogni avvio; rilevamento stasi (grid statica, oscillatori periodici, estinzione) con iniezione automatica di blinker casuali per mantenere la simulazione viva.
- **Ciclo schermi toggle**: Il pulsante "Ciclo schermi" nell'interfaccia web è ora un toggle ON/OFF con feedback visivo (colore ambra quando attivo). Selezionare una schermata specifica dall'interfaccia web disattiva automaticamente il ciclo.
- **Endpoint `/cycle` migliorato**: Alterna lo stato del ciclo e restituisce `{"cycle": true/false}`; `/status` include ora il campo `cycle`.
- **Endpoint aggiuntivi**: `/golreset` (re-popola il Game of Life), `/rssscreen` (commuta al feed RSS).
- **Meteo**: Corretti campi `description` e `icon` che comparivano come `null` per via del filtro JSON troppo piccolo.

Configurazione scheda in PlatformIO

Questo progetto usa la board `esp32-s3-fh4r2` (4 MB flash QIO + 2 MB PSRAM). Per farla riconoscere a PlatformIO hai due opzioni:

1) Usare il repository esterno con molte board predefinite:
- Repo: https://github.com/sivar2311/platformio_boards
- Segui le istruzioni del repo per installare/aggiornare le definizioni. Dopo l’installazione, la voce `board = esp32-s3-fh4r2` nel file `platformio.ini` funzionerà senza altre modifiche.

2) Usare il file incluso in questo repo:
- Il JSON della board è in `info/esp32-s3-fh4r2.json`.
- Opzione A (consigliata per progetto locale): aggiungi in cima a `platformio.ini`:

```
[platformio]
boards_dir = info
```

- Opzione B: copia `info/esp32-s3-fh4r2.json` nella tua cartella `boards` personalizzata di PlatformIO (vedi documentazione `boards_dir`).

File `platformio.ini`

- Ambiente attivo: `[env:esp32-s3-fh4r2]`
- Piattaforma: `espressif32`
- Framework: `arduino`
- File system: **LittleFS** con partizioni da `partitions.csv`
- Flag principali: USB CDC su boot abilitato

Caricamento firmware e filesystem

- Firmware: compila e carica con PlatformIO (Upload)
- File web: carica la partizione LittleFS con “Upload Filesystem Image” (**Nota: necessario ricaricare se si proviene da SPIFFS**)

Contenuti consigliati in `data/`

- `INDEX.HTM` pagina principale (home)
- `SETUP.HTM` pagina di configurazione Wi‑Fi/mdns
- `WIFI_SID.TXT` e `WIFI_PWD.TXT` credenziali Wi‑Fi
- `MDNS_NM.TXT` nome mDNS/SSID AP di fallback (default `MECSVEGLIA`)
- `SCREEN.TXT` numero schermo di avvio (0..7, opzionale)
- `METEO_API.TXT` query string di OpenWeatherMap dopo il `?` (esempio: `id=3175384&cnt=1&units=metric&lang=it&appid=XXXX`)

Uso rapido

- Avvio in STA: se presenti SSID/PWD su LittleFS, il dispositivo si collega al Wi‑Fi e risponde a `http://<mdns>.local` (es. `http://MECSVEGLIA.local`).
- Fallback AP: se mancano credenziali o la rete non è disponibile, crea un AP aperto con SSID `<mdns>` e mostra l’IP sul display.
- Endpoint utili: `/` home, `/setup`, `/status` (JSON con `cycle`), `/wifi`, `/time`, `/eyes`, `/meteo`, `/tri`, `/gol`, `/golreset`, `/cycle` (toggle), `/rssscreen`, `/oled`, `/rss`.

Hardware

- Display SSD1306 I2C 128×32 a 3.3V, indirizzo 0x3C (default). Collega SDA/SCL ai pin I2C della board come da pinout.
- Pulsante su `GPIO12` in modalità pull‑up interna: collega a GND per avanzare manualmente tra gli schermi.

Stampa 3D

- I modelli sono nella cartella `stl/`. Scegli l’alloggiamento più adatto (`mec01.stl`, `mac02.stl`) e stampa con materiale a tua scelta.

Note

- Il progetto abilita OTA: gli aggiornamenti possono essere inviati via rete quando il dispositivo è connesso.
- Il display si attenua di notte (22:00–06:59) tramite `display.dim(true)`.
- L'inversione periodica anti burn‑in è disabilitata.

