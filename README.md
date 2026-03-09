# ESP32 WROOM + NeoMatrix 8x32 CKPool monitor

Firmware Arduino: `hacs.ino`.

## Migliorie implementate
- Connessione Wi-Fi **solo STA** (no AP).
- Fetch dati pool con fallback URL e gestione HTTPS su ESP32 (`WiFiClientSecure` + redirect).
- 6 schermate fisse: `5M`, `1H`, `1D`, `7D`, `TS`, `BS`.
- Formato fisso display: **2 caratteri + 4 cifre**.
- Rendering testo condensato per mostrare sempre tutti i 6 caratteri su matrice 8x32.
- Web UI ridisegnata in stile dashboard professionale.
- Configurazione per-schermata del colore (RGB separato per `5M/1H/1D/7D/TS/BS`).
- Configurazioni salvate in NVS: colori, velocità rotazione, luminosità, modo notte, auto-reset, dimensione/spaziatura testo.

## Parametri richiesti inclusi
- `PIN_LED = 13`
- `#define MATRIX_FLAGS (NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG)`
- SSID: `GieMa`
- Password: `ernestino`
- Wallet: `bc1que63wfl066wx9d3mq0fjz9tr7pqslg0aww4h39`
