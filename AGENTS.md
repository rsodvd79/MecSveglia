# Guida Agente MecSveglia

## Panoramica del Progetto
Orologio/compagno da scrivania basato su ESP32-S3 con display OLED, WiFi, ora NTP, meteo, notizie RSS e interfaccia web.

## Directory Principali
- `src/` - Codice sorgente principale
- `data/` - File caricati su LittleFS (pagine web, file di configurazione)
- `info/` - Definizioni della scheda e pinout
- `stl/` - Modelli stampabili in 3D
- `test/` - File di test

## Comandi Essenziali

### PlatformIO
- Compila: `pio run`
- Carica firmware: `pio run --target upload`
- Carica filesystem: `pio run --target uploadfs`
- Pulisci: `pio run --target clean`
- Monitor: `pio device monitor`

### Note sul Filesystem
- **LittleFS utilizzato** (configurato come `spiffs` in platformio.ini ma mappato a LittleFS tramite `#define LittleFS SPIFFS` in main.cpp)
- File critici in `data/`:
  - `INDEX.HTM` - Pagina principale
  - `SETUP.HTM` - Configurazione WiFi
  - `WIFI_SID.TXT` / `WIFI_PWD.TXT` - Credenziali WiFi
  - `MDNS_NM.TXT` - Nome mDNS (predefinito: MECSVEGLIA)
  - `SCREEN.TXT` - Schermo di avvio (0-7)
  - `METEO_API.TXT` - Stringa di query OpenWeatherMap
  - `RSS.TXT` - Categoria RSS
  - `TIME_ZONE.TXT` - Stringa del fuso orario

## Flusso di Lavoro di Sviluppo
1. Modifica il sorgente in `src/`
2. Modifica/crea file in `data/` secondo necessità
3. Compila e carica il firmware: `pio run --target upload`
4. Carica l'immagine del filesystem: `pio run --target uploadfs` (necessario quando cambiano i dati)
5. Monitora l'output: `pio device monitor`

## Dettagli Importanti di Implementazione

### Task di Rete
- Task FreeRTOS gestisce gli aggiornamenti RSS/meteo per evitare il blocco del display
- Situato in `main.cpp`: funzione `networkTask()`
- Aggiorna RSS ogni ora, meteo ogni 30 minuti

### Schermi di Visualizzazione (0-7)
0: Stato WiFi
1: Ora/data
2: Animazioni occhi
3: Meteo
4: Game of Life
5: Triangolo animato
6: Notizie RSS
7: Display automatico ciclico

### Pulsante (GPIO12)
- Avanzamento schermo manuale (pressione breve)
- Pressione lunga attiva l'inversione del display (protezione contro il burn-in)

### Endpoint HTTP
- `/` - Pagina principale
- `/setup` - Configurazione WiFi
- `/status` - Stato JSON
- `/time` - Forza schermo ora
- `/eyes` - Forza schermo occhi
- `/meteo` - Forza schermo meteo
- `/gol` - Forza Game of Life
- `/tri` - Forza triangolo
- `/cycle` - Attiva/disattiva auto-ciclo
- `/rss` - Controllo feed RSS
- `/oled` - Buffer display grezzo
- `/wifi` - Forza schermo WiFi
- `/timezone` - Configurazione fuso orario

### Aggiornamenti OTA
- Abilitati tramite ArduinoOTA
- Hostname corrisponde al nome mDNS
- Richiede connessione di rete

## Configurazione di Compilazione
- Scheda: `esp32-s3-fh4r2` (4MB flash + 2MB PSRAM)
- Framework: Arduino
- USB CDC abilitato all'avvio
- Librerie gestite tramite platformio.ini:
  - Adafruit SSD1306
  - Adafruit GFX
  - Adafruit NeoPixel
  - ArduinoJson

## Verifica
- Compilazione riuscita mostra nessun errore
- Il dispositivo si connette al WiFi quando presenti le credenziali
- Torna alla modalità AP quando mancano le credenziali
- Risponditore mDNS attivo come `<mdns_name>.local`
- Il display mostra gli schermi appropriati in base allo stato