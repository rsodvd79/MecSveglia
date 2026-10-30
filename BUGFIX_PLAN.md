# Piano correzione bug

Documento operativo per la prossima sessione. Applicare modifiche piccole e
separate, mantenendo invariato il comportamento non coinvolto.

## 1. Ripristinare il supporto OTA firmware

**Priorita:** alta

**File:** `partitions.csv`

La tabella contiene `otadata` e un solo slot applicativo (`ota_0`). ArduinoOTA
non puo aggiornare in sicurezza la partizione dalla quale il firmware e in
esecuzione.

### Modifica

- Definire due partizioni applicative di uguale dimensione: `ota_0` e `ota_1`.
- Conservare `otadata`.
- Ridimensionare LittleFS per fare rientrare entrambe le applicazioni nei 4 MB
  di flash.
- Verificare che ogni slot sia piu grande del firmware compilato, lasciando un
  margine ragionevole.
- Non modificare `board_build.partitions = partitions.csv`.

Una possibile distribuzione deve rispettare questa struttura:

```text
nvs
otadata
ota_0
ota_1
spiffs
```

Calcolare offset e dimensioni solo dopo aver verificato la dimensione reale
dell'immagine prodotta da PlatformIO.

### Verifica

1. Eseguire `pio run`.
2. Controllare che il firmware entri in entrambi gli slot OTA.
3. Eseguire un aggiornamento ArduinoOTA reale.
4. Verificare che il dispositivo si riavvii con il nuovo firmware.
5. Eseguire `pio run --target uploadfs` e verificare la lettura dei file web.

## 2. Aggiungere retry brevi per RSS e meteo

**Priorita:** media

**File:** `src/main.cpp`, eventualmente `src/meteo.h` e `src/meteo.cpp`

In `networkTask`, i timer e i flag forzati vengono consumati prima di conoscere
l'esito della richiesta. Dopo un errore RSS si puo attendere un'ora; dopo un
errore meteo fino a 30 minuti.

### Modifica

- Usare due timestamp rollover-safe per programmare un retry dopo un errore.
- Retry suggerito: 60 secondi.
- Non eseguire RSS e meteo nello stesso tick, per conservare l'attuale
  limitazione dei picchi TLS.
- `classRss::refresh()` restituisce gia `bool`: usare il valore restituito.
- Modificare `classMeteo::Update()` affinche restituisca `bool`, con `true`
  soltanto dopo risposta HTTP valida e JSON deserializzato correttamente.
- Dopo un successo, cancellare il retry relativo.
- Dopo un errore, mantenere i dati precedenti e programmare il retry.
- Usare sempre sottrazioni basate su `millis()`; non confrontare direttamente
  timestamp assoluti.

### Verifica

1. Avviare il dispositivo con rete disponibile ma endpoint irraggiungibile.
2. Ripristinare l'endpoint senza disconnettere il Wi-Fi.
3. Verificare un nuovo tentativo entro circa 60 secondi.
4. Verificare che il normale intervallo resti un'ora per RSS e 30 minuti per
   meteo dopo un aggiornamento riuscito.

## 3. Eliminare la data race del callback SNTP

**Priorita:** media

**File:** `src/main.cpp`

Il callback registrato con `sntp_set_time_sync_notification_cb()` viene
eseguito dal task lwIP. Attualmente scrive `timeSyncedOnce` e
`lastSuccessfulSync` mentre il loop principale legge e modifica gli stessi
valori.

### Modifica

- Rendere il callback minimale: deve soltanto impostare un
  `std::atomic<bool>` che segnali una sincronizzazione completata.
- Spostare nel loop, dentro `maintainTimeSync()`, la lettura di `time(nullptr)`
  e l'aggiornamento di `timeSyncedOnce` e `lastSuccessfulSync`.
- Non rendere atomico direttamente `time_t` se non necessario.
- Non usare mutex, log o operazioni bloccanti nel callback lwIP.

Schema suggerito:

```cpp
static std::atomic<bool> timeSyncCompleted{false};

sntp_set_time_sync_notification_cb([](struct timeval *) {
  timeSyncCompleted.store(true);
});
```

`maintainTimeSync()` deve consumare il flag con `exchange(false)`.

### Verifica

- Compilare senza cast o accessi `volatile`.
- Verificare da log che ogni callback aggiorni una sola volta
  `lastSuccessfulSync`.
- Lasciare il dispositivo collegato abbastanza a lungo da osservare una
  sincronizzazione successiva.

## 4. Rimontare LittleFS dopo un errore OTA filesystem

**Priorita:** media

**File:** `src/main.cpp`

Durante un aggiornamento filesystem viene chiamato `LittleFS.end()`. Se OTA
fallisce, `onError` non rimonta il filesystem.

### Modifica

- Memorizzare se il comando OTA corrente e `U_FS`.
- In `ArduinoOTA.onError`, se LittleFS era stato smontato, chiamare
  `LittleFS.begin(false)`.
- Registrare su seriale l'eventuale errore di rimontaggio.
- Azzerare lo stato anche in `onEnd`.
- Non formattare automaticamente il filesystem durante il recupero:
  `LittleFS.begin(true)` potrebbe cancellare configurazione e pagine web.

### Verifica

1. Avviare un OTA filesystem e interromperlo intenzionalmente.
2. Verificare che `/`, `/setup` e `/favicon.ico` continuino a leggere
   LittleFS senza riavviare.
3. Verificare che un successivo OTA filesystem possa essere avviato.

## 5. Non alterare SSID e password Wi-Fi

**Priorita:** media

**File:** `src/main.cpp`

`trim()` modifica credenziali valide con spazi iniziali o finali. Inoltre il
POST tronca l'SSID a 31 caratteri, mentre un SSID puo contenere fino a 32 byte.

### Modifica

- Rimuovere `trim()` da SSID e password sia durante il caricamento sia nel POST.
- Rimuovere il troncamento silenzioso.
- Rifiutare con HTTP 400 un SSID oltre 32 byte.
- Accettare password vuota per reti aperte.
- Per WPA/WPA2, accettare passphrase da 8 a 63 caratteri; non correggerle
  automaticamente.
- Separare il concetto di file password presente dal concetto di password non
  vuota, cosi una rete aperta puo essere considerata configurata.
- Continuare a usare `trim()` per il nome mDNS, ma validarlo separatamente.

### Verifica

- Connessione a un SSID di 32 byte.
- Connessione con password contenente uno spazio iniziale o finale.
- Connessione a una rete aperta.
- Risposta HTTP 400 per credenziali fuori dai limiti, senza sovrascrivere i
  file validi esistenti.

## 6. Correggere la validazione della categoria RSS

**Priorita:** bassa

**File:** `src/main.cpp`, `src/rss.cpp`

`normalizeCategory()` restituisce sempre una categoria valida, usando
`tecnologia` come fallback. Validare il risultato normalizzato rende quindi
irraggiungibile il ramo che segnala un file non valido.

### Modifica

- Copiare il valore ricevuto, eseguire `trim()` e validare quel valore.
- Chiamare `normalizeCategory()` solo dopo che `isValidCategory()` ha restituito
  `true`.
- Applicare lo stesso flusso nel caricamento da file e in `handleRssPost()`.
- Se il file contiene un valore non valido, usare la categoria predefinita e
  riscrivere `RSS.TXT`.
- Se il POST contiene un valore non valido, rispondere HTTP 400 senza cambiare
  file o stato in memoria.

### Verifica

- `sport`, `SPORT` e ` sport ` devono diventare `sport`.
- Un valore sconosciuto nel file deve essere sostituito con `tecnologia`.
- Un valore sconosciuto via HTTP deve ricevere 400.

## 7. Limitare la qualita RSSI a 0-100

**Priorita:** bassa

**File:** `src/main.cpp`

La formula corrente puo produrre valori superiori al 100% per segnali migliori
di `-50 dBm`.

### Modifica

Usare soglie coerenti, ad esempio:

```cpp
if (dBm <= -100) return 0;
if (dBm >= -50) return 100;
return 2 * (dBm + 100);
```

### Verifica

Controllare almeno questi casi:

| RSSI | Risultato |
|---:|---:|
| -110 dBm | 0% |
| -100 dBm | 0% |
| -75 dBm | 50% |
| -50 dBm | 100% |
| -30 dBm | 100% |

## Ordine di esecuzione consigliato

1. Partizioni OTA.
2. Retry RSS/meteo.
3. Sincronizzazione SNTP.
4. Recupero LittleFS dopo OTA.
5. Validazione Wi-Fi.
6. Validazione RSS.
7. Conversione RSSI.

## Controllo finale

Eseguire:

```bash
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

Non esistono unit test nel repository. Oltre alla compilazione, verificare su
ESP32-S3: avvio STA, fallback AP, ritorno AP-STA, NTP, refresh RSS/meteo,
lettura della web UI, OTA firmware e OTA filesystem.
