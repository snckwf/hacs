# Revisione codice `Meteo_16x32` (Arduino IDE / ESP32)

## Esito rapido

Il codice è **strutturalmente valido** per Arduino IDE con core ESP32 recente (2.x/3.x), ma ci sono alcuni punti da correggere o semplificare per migliorare robustezza e pulizia.

## Correzioni consigliate (alta priorità)

1. **Credenziali in chiaro**
   - Spostare SSID/password WiFi, API key OpenWeather e password OTA fuori dal file sorgente (es. `secrets.h` escluso dal versionamento).

2. **`stateJson()` con `String(rssi).c_str()` dentro `snprintf`**
   - Evitare puntatori temporanei da `String` in argomenti C-style.
   - Usare un buffer locale (`char rssiBuf[12]`) e passare quello a `snprintf`.

3. **Watchdog dipendente da versione core**
   - Il blocco con `esp_task_wdt_config_t` è corretto per ESP32 core 3.x, ma può rompere compilazione su 2.x.
   - Per compatibilità, aggiungere `#if ESP_ARDUINO_VERSION_MAJOR >= 3` oppure mantenere una variante fallback per 2.x.

4. **Timeout/fetch meteo**
   - Parsing JSON manuale funziona ma è fragile se il payload cambia.
   - Se lo sketch cresce, valutare `ArduinoJson` con `StaticJsonDocument` limitato.

## Pulizia codice inutile / migliorabile

1. **`volatile bool g_dirty`**
   - Se non viene modificata da ISR/task concorrenti, `volatile` è inutile.

2. **Costanti mai variate a runtime**
   - Valutare `constexpr` al posto di `const` per costanti compile-time (`FORCE_NIGHT_SECONDS_DEFAULT`, `MIN_FRAME_MS`, ecc.).

3. **Ridurre duplicazione colori/glyph/layout**
   - Alcuni magic numbers (`TEXT_W = 11`, coordinate varie) possono diventare costanti nominate.

4. **OTA begin condizionato al WiFi in setup**
   - Va bene così, ma ricordare che se al boot WiFi non sale, OTA resta spento fino a reboot.

## Checklist Arduino IDE

- Board: **ESP32 Dev Module / ESP32-WROOM-32**.
- Librerie richieste:
  - `FastLED`
  - (core ESP32 include `WiFi`, `WebServer`, `Preferences`, `ArduinoOTA`)
- Impostazioni consigliate:
  - Partition Scheme con spazio app adeguato OTA (es. `Default 4MB with spiffs` o simile con OTA).
  - Upload Speed e Flash Frequency standard del modulo.

## Patch minima consigliata (estratto)

```cpp
// prima
volatile bool g_dirty = true;

// dopo
bool g_dirty = true;
```

```cpp
// in stateJson(), prima: wifi_ok ? String(rssi).c_str() : "null"
char rssiBuf[12] = "null";
if (wifi_ok) snprintf(rssiBuf, sizeof(rssiBuf), "%d", rssi);
...
snprintf(json, sizeof(json),
  ...,
  rssiBuf,
  ...);
```

```cpp
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms = 10000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_cfg);
  esp_task_wdt_add(NULL);
#else
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
#endif
```

## Conclusione

Lo sketch è già molto completo e ben organizzato per un uso reale su matrice 16x32.
Con i piccoli interventi sopra (soprattutto secrets, `stateJson` e compatibilità watchdog), risulta più pulito e più sicuro da mantenere.
