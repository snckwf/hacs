// Meteo_16x32 - versione pulita e completa
// ESP32-WROOM32 + FastLED + WebServer + NVS + NTP + OpenWeather + ArduinoOTA
// Matrice 16x32 = 2 pannelli 8x32 (alto + basso)

struct Icon16x16PalAnim;

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <FastLED.h>
#include <time.h>
#include <math.h>
#include <pgmspace.h>
#include "esp_task_wdt.h"
#include <ArduinoOTA.h>

// =========================
// Secrets (metti in secrets.h e non versionare)
// =========================
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_PASS"
#endif
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "Meteo16x32"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "CHANGE_ME"
#endif
#ifndef OW_API_KEY_DEFAULT
#define OW_API_KEY_DEFAULT "YOUR_OPENWEATHER_KEY"
#endif
#ifndef OW_LAT_DEFAULT
#define OW_LAT_DEFAULT "45.80900"
#endif
#ifndef OW_LON_DEFAULT
#define OW_LON_DEFAULT "13.44175"
#endif

// =========================
// Matrix
// =========================
constexpr uint8_t PANEL_W = 32;
constexpr uint8_t PANEL_H = 8;
constexpr uint8_t MATRIX_W = 32;
constexpr uint8_t MATRIX_H = 16;
constexpr uint16_t PANEL_LEDS = PANEL_W * PANEL_H;
constexpr uint16_t NUM_LEDS = MATRIX_W * MATRIX_H;

constexpr uint8_t DATA_PIN = 5;
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
WebServer server(80);
Preferences prefs;

// =========================
// Settings
// =========================
uint8_t g_brightness = 18; // 1..255
CRGB g_colTemp = CRGB(255, 80, 0);
CRGB g_colHum = CRGB(0, 160, 255);
CRGB g_colBg = CRGB::Black;

uint8_t g_nightBrightnessMin = 5;
uint8_t g_nightBrightnessPct = 35;
uint8_t g_nightDimPct = 60;
uint8_t g_animSpeedPct = 45;

uint16_t g_dayStartMin = 7 * 60;
uint16_t g_nightStartMin = 21 * 60;
bool g_isNight = false;

uint16_t g_owIntervalMin = 10;
uint16_t g_autoResetHours = 0;

bool g_forceNightPreview = false;
unsigned long g_forceNightUntilMs = 0;
constexpr uint16_t FORCE_NIGHT_SECONDS_DEFAULT = 15;

unsigned long meteoLastMs = 0;
float g_tempC = NAN;
int g_humidity = -1;
char g_icon[4] = "03d";
bool g_meteoOk = false;

constexpr unsigned long MIN_FRAME_MS = 33;
bool g_dirty = true;

uint8_t lastAlpha8 = 255;
bool lastNight = false;
int lastT = 99999;
int lastHum = 9999;
char lastIcon[4] = "xxx";
uint8_t lastEffBr = 255;
unsigned long lastRenderMs = 0;

unsigned long lastWifiOkMs = 0;
unsigned long lastStateServeMs = 0;
unsigned long lastHealthMs = 0;
unsigned long bootMs = 0;

constexpr unsigned long HEALTH_PERIOD_MS = 5000;
constexpr unsigned long WIFI_GRACE_MS = 30000;
constexpr unsigned long HARD_RESTART_MS = 0;

String OW_API_KEY = OW_API_KEY_DEFAULT;
String OW_LAT = OW_LAT_DEFAULT;
String OW_LON = OW_LON_DEFAULT;
String OW_LANG = "it";
String OW_UNITS = "metric";

struct Icon16x16PalAnim {
  const uint8_t* f0;
  const uint8_t* f1;
  const CRGB* pal;
};

constexpr uint8_t ICON_W = 16;
constexpr uint8_t ICON_H = 16;

const CRGB PAL_DAY[8] = {
  CRGB::Black,
  CRGB::White,
  CRGB(220, 230, 245),
  CRGB(255, 220, 80),
  CRGB(120, 200, 255),
  CRGB(255, 235, 40),
  CRGB(235, 245, 255),
  CRGB(170, 180, 195)
};

const CRGB PAL_NIGHT[8] = {
  CRGB::Black,
  CRGB::White,
  CRGB(190, 210, 245),
  CRGB(220, 235, 255),
  CRGB(80, 140, 230),
  CRGB(255, 235, 40),
  CRGB(235, 245, 255),
  CRGB(125, 160, 220)
};

// Icone semplificate (16x16) per mantenere sketch leggero ma completo.
const uint8_t ICON_CLEAR_F0[256] PROGMEM = {
  0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0, 0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0,
  0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0, 0,0,0,0,3,3,3,3,3,3,3,3,0,0,0,0,
  0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0, 0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0,
  0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0, 0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,
  0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0, 0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0,
  0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0, 0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,
  0,0,0,0,3,3,3,3,3,3,3,3,0,0,0,0, 0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0,
  0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
const uint8_t ICON_CLEAR_F1[256] PROGMEM = { // leggero pulse
  0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0, 0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0,
  0,0,0,0,3,3,3,3,3,3,3,3,0,0,0,0, 0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,
  0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0, 0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,
  0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0, 0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,
  0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0, 0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,
  0,0,3,3,3,3,3,3,3,3,3,3,3,3,0,0, 0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,
  0,0,0,0,3,3,3,3,3,3,3,3,0,0,0,0, 0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0,
  0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0, 0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0
};

const uint8_t ICON_CLOUD_F0[256] PROGMEM = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0,
  0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0, 0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,
  0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0, 0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0, 0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,
  0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0, 0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
const uint8_t ICON_CLOUD_F1[256] PROGMEM = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,2,2,2,0,0,0,0,0,0,0,
  0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0, 0,0,0,0,2,2,2,2,2,2,2,0,0,0,0,0,
  0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0, 0,0,2,2,2,2,2,2,2,2,2,2,2,0,0,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0, 0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

const uint8_t ICON_RAIN_F0[256] PROGMEM = {
  0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0, 0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,
  0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0, 0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,
  0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0, 0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0, 0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,4,0,0,4,0,0,4,0,0,4,0,0,0,
  0,0,0,0,0,4,0,0,4,0,0,4,0,0,4,0, 0,0,4,0,0,0,4,0,0,4,0,0,4,0,0,0,
  0,0,0,0,4,0,0,4,0,0,4,0,0,4,0,0, 0,0,0,4,0,0,4,0,0,4,0,0,4,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
const uint8_t ICON_RAIN_F1[256] PROGMEM = {
  0,0,0,0,0,0,0,2,2,0,0,0,0,0,0,0, 0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,
  0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0, 0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,
  0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0, 0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0, 0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,4,0,0,4,0,0,4,0,0,4,0,0,
  0,0,0,4,0,0,4,0,0,4,0,0,4,0,0,0, 0,0,0,0,0,4,0,0,4,0,0,4,0,0,4,0,
  0,0,4,0,0,0,4,0,0,4,0,0,4,0,0,0, 0,0,0,0,4,0,0,4,0,0,4,0,0,4,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

const Icon16x16PalAnim ICON_DAY_CLEAR = { ICON_CLEAR_F0, ICON_CLEAR_F1, PAL_DAY };
const Icon16x16PalAnim ICON_NIGHT_CLEAR = { ICON_CLEAR_F0, ICON_CLEAR_F1, PAL_NIGHT };
const Icon16x16PalAnim ICON_DAY_CLOUD = { ICON_CLOUD_F0, ICON_CLOUD_F1, PAL_DAY };
const Icon16x16PalAnim ICON_NIGHT_CLOUD = { ICON_CLOUD_F0, ICON_CLOUD_F1, PAL_NIGHT };
const Icon16x16PalAnim ICON_DAY_RAIN = { ICON_RAIN_F0, ICON_RAIN_F1, PAL_DAY };
const Icon16x16PalAnim ICON_NIGHT_RAIN = { ICON_RAIN_F0, ICON_RAIN_F1, PAL_NIGHT };

// =========================
// Utils
// =========================
static inline uint8_t hex2b(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return 0;
}

CRGB parseHexColor(const String& s, const CRGB fallback) {
  if (s.length() != 7 || s[0] != '#') return fallback;
  const uint8_t r = (hex2b(s[1]) << 4) | hex2b(s[2]);
  const uint8_t g = (hex2b(s[3]) << 4) | hex2b(s[4]);
  const uint8_t b = (hex2b(s[5]) << 4) | hex2b(s[6]);
  return CRGB(r, g, b);
}

static inline void toHexColorBuf(const CRGB& c, char out7[8]) {
  snprintf(out7, 8, "#%02X%02X%02X", c.r, c.g, c.b);
}

static inline CRGB scaleColorPct(CRGB in, uint8_t pct) {
  in.nscale8_video(static_cast<uint8_t>((static_cast<uint16_t>(pct) * 255) / 100));
  return in;
}

static inline uint8_t brPctFrom255(const uint8_t b) {
  uint16_t p = static_cast<uint16_t>(b) * 100U + 127U;
  p /= 255U;
  if (p < 1) p = 1;
  if (p > 100) p = 100;
  return static_cast<uint8_t>(p);
}

static inline uint8_t br255FromPct(uint8_t p) {
  if (p < 1) p = 1;
  if (p > 100) p = 100;
  uint16_t b = static_cast<uint16_t>(p) * 255U + 50U;
  b /= 100U;
  if (b < 1) b = 1;
  if (b > 255) b = 255;
  return static_cast<uint8_t>(b);
}

bool timeIsValid() { return time(nullptr) > 1700000000; }

static inline void minToHHMMBuf(const uint16_t m, char out5[6]) {
  snprintf(out5, 6, "%02u:%02u", static_cast<unsigned>((m / 60) % 24), static_cast<unsigned>(m % 60));
}

static inline uint16_t hhmmToMinSafe(const String& hhmm, const uint16_t fallback) {
  if (hhmm.length() != 5 || hhmm[2] != ':') return fallback;
  const int hh = hhmm.substring(0, 2).toInt();
  const int mm = hhmm.substring(3, 5).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return fallback;
  return static_cast<uint16_t>(hh * 60 + mm);
}

static inline uint16_t XY_panel(const uint8_t x, const uint8_t y) {
  const uint8_t yy = (x & 1) ? (PANEL_H - 1 - y) : y;
  return static_cast<uint16_t>(x) * PANEL_H + yy;
}

uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_W || y >= MATRIX_H) return 0;
  const uint8_t panel = (y >= PANEL_H) ? 1 : 0;
  y = (panel == 0) ? y : (y - PANEL_H);

  if (panel == 1) {
    x = (PANEL_W - 1) - x;
    y = (PANEL_H - 1) - y;
  }
  return static_cast<uint16_t>(panel) * PANEL_LEDS + XY_panel(x, y);
}

static inline void setPixel(const uint8_t x, const uint8_t y, const CRGB& c) {
  if (x >= MATRIX_W || y >= MATRIX_H) return;
  leds[XY(x, y)] = c;
}

bool isNightBySchedule(const uint16_t nowMin) {
  if (g_dayStartMin == g_nightStartMin) return false;
  if (g_dayStartMin < g_nightStartMin) return (nowMin >= g_nightStartMin) || (nowMin < g_dayStartMin);
  return (nowMin >= g_nightStartMin) && (nowMin < g_dayStartMin);
}

void updateNightFlag_Auto() {
  if (timeIsValid()) {
    struct tm t{};
    const time_t now = time(nullptr);
    localtime_r(&now, &t);
    const uint16_t nowMin = static_cast<uint16_t>(t.tm_hour * 60 + t.tm_min);
    g_isNight = isNightBySchedule(nowMin);
  } else {
    g_isNight = (g_icon[2] == 'n');
  }
}

void updateNightFlag() {
  if (g_forceNightPreview) {
    if (g_forceNightUntilMs != 0 && millis() > g_forceNightUntilMs) {
      g_forceNightPreview = false;
      g_forceNightUntilMs = 0;
      updateNightFlag_Auto();
      g_dirty = true;
    } else {
      g_isNight = true;
    }
    return;
  }
  updateNightFlag_Auto();
}

static inline uint8_t effectiveBrightness() {
  if (!g_isNight) return g_brightness;
  uint16_t b = static_cast<uint16_t>(g_brightness) * g_nightBrightnessPct;
  b = (b + 50) / 100;
  if (b < g_nightBrightnessMin) b = g_nightBrightnessMin;
  if (b > 255) b = 255;
  return static_cast<uint8_t>(b);
}

// =========================
// Prefs
// =========================
void loadPrefs() {
  prefs.begin("meteo16x32", true);
  g_brightness = prefs.getUChar("br", g_brightness);
  g_colTemp = CRGB(prefs.getUChar("tr", g_colTemp.r), prefs.getUChar("tg", g_colTemp.g), prefs.getUChar("tb", g_colTemp.b));
  g_colHum  = CRGB(prefs.getUChar("hr", g_colHum.r), prefs.getUChar("hg", g_colHum.g), prefs.getUChar("hb", g_colHum.b));
  g_colBg   = CRGB(prefs.getUChar("br0", g_colBg.r), prefs.getUChar("bg0", g_colBg.g), prefs.getUChar("bb0", g_colBg.b));
  g_dayStartMin = prefs.getUShort("ds", g_dayStartMin);
  g_nightStartMin = prefs.getUShort("ns", g_nightStartMin);
  g_owIntervalMin = prefs.getUShort("owm", g_owIntervalMin);
  g_autoResetHours = prefs.getUShort("arh", g_autoResetHours);
  g_nightBrightnessPct = prefs.getUChar("nbp", g_nightBrightnessPct);
  g_nightBrightnessMin = prefs.getUChar("nbm", g_nightBrightnessMin);
  g_nightDimPct = prefs.getUChar("ndp", g_nightDimPct);
  g_animSpeedPct = prefs.getUChar("asp", g_animSpeedPct);
  prefs.end();

  g_dayStartMin = constrain(g_dayStartMin, static_cast<uint16_t>(0), static_cast<uint16_t>(1439));
  g_nightStartMin = constrain(g_nightStartMin, static_cast<uint16_t>(0), static_cast<uint16_t>(1439));
  g_owIntervalMin = constrain(g_owIntervalMin, static_cast<uint16_t>(1), static_cast<uint16_t>(120));
  g_autoResetHours = constrain(g_autoResetHours, static_cast<uint16_t>(0), static_cast<uint16_t>(168));
  g_nightBrightnessPct = constrain(g_nightBrightnessPct, static_cast<uint8_t>(1), static_cast<uint8_t>(100));
  g_nightBrightnessMin = constrain(g_nightBrightnessMin, static_cast<uint8_t>(1), static_cast<uint8_t>(80));
  g_nightDimPct = constrain(g_nightDimPct, static_cast<uint8_t>(1), static_cast<uint8_t>(100));
  g_animSpeedPct = constrain(g_animSpeedPct, static_cast<uint8_t>(1), static_cast<uint8_t>(100));
  if (g_brightness < 1) g_brightness = 1;
}

void savePrefs() {
  prefs.begin("meteo16x32", false);
  prefs.putUChar("br", g_brightness);
  prefs.putUChar("tr", g_colTemp.r); prefs.putUChar("tg", g_colTemp.g); prefs.putUChar("tb", g_colTemp.b);
  prefs.putUChar("hr", g_colHum.r);  prefs.putUChar("hg", g_colHum.g);  prefs.putUChar("hb", g_colHum.b);
  prefs.putUChar("br0", g_colBg.r);  prefs.putUChar("bg0", g_colBg.g);  prefs.putUChar("bb0", g_colBg.b);
  prefs.putUShort("ds", g_dayStartMin); prefs.putUShort("ns", g_nightStartMin);
  prefs.putUShort("owm", g_owIntervalMin); prefs.putUShort("arh", g_autoResetHours);
  prefs.putUChar("nbp", g_nightBrightnessPct); prefs.putUChar("nbm", g_nightBrightnessMin);
  prefs.putUChar("ndp", g_nightDimPct); prefs.putUChar("asp", g_animSpeedPct);
  prefs.end();
}

// =========================
// OpenWeather parse (manuale)
// =========================
static bool extractJsonNumberC(const char* s, const char* key, float& outVal) {
  const char* k = strstr(s, key); if (!k) return false;
  k = strchr(k, ':'); if (!k) return false;
  k++;
  while (*k == ' ') k++;
  char* endptr = nullptr;
  outVal = strtof(k, &endptr);
  return endptr && endptr != k;
}

static bool extractJsonIntC(const char* s, const char* key, int& outVal) {
  float f;
  if (!extractJsonNumberC(s, key, f)) return false;
  outVal = static_cast<int>(lroundf(f));
  return true;
}

static bool extractIconC(const char* s, char iconOut3[4]) {
  const char* k = strstr(s, "\"icon\"");
  if (!k) return false;
  k = strchr(k, ':'); if (!k) return false;
  k = strchr(k, '"'); if (!k) return false;
  k++;
  if (strlen(k) < 3) return false;
  iconOut3[0] = k[0]; iconOut3[1] = k[1]; iconOut3[2] = k[2]; iconOut3[3] = '\0';
  return true;
}

bool fetchMeteo() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (OW_API_KEY.length() < 10) return false;

  const char* host = "api.openweathermap.org";
  const String url = "/data/2.5/weather?lat=" + OW_LAT + "&lon=" + OW_LON + "&appid=" + OW_API_KEY + "&units=" + OW_UNITS + "&lang=" + OW_LANG;

  WiFiClient client;
  client.setTimeout(8000);
  if (!client.connect(host, 80)) return false;

  client.print(String("GET ") + url + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n\r\n");

  while (client.connected()) {
    const String line = client.readStringUntil('\n');
    if (line == "\r") break;
    yield();
  }

  static char body[4096];
  size_t idx = 0;
  const unsigned long t0 = millis();
  while (client.connected() || client.available()) {
    while (client.available()) {
      const char c = static_cast<char>(client.read());
      if (idx < sizeof(body) - 1) body[idx++] = c;
    }
    if (millis() - t0 > 8500) break;
    yield();
  }
  body[idx] = '\0';
  client.stop();

  float t;
  int h;
  char ic[4];
  if (!extractJsonNumberC(body, "\"temp\"", t)) return false;
  if (!extractJsonIntC(body, "\"humidity\"", h)) return false;
  if (!extractIconC(body, ic)) return false;

  g_tempC = t;
  g_humidity = h;
  memcpy(g_icon, ic, 4);
  updateNightFlag();
  return true;
}

unsigned long animPeriodMs() { return map(g_animSpeedPct, 1, 100, 5600, 700); }
uint8_t computeAlpha8() {
  const unsigned long period = animPeriodMs();
  const uint32_t t = millis() % period;
  const uint32_t half = period / 2;
  const uint32_t a = (t < half) ? t : (period - t);
  return static_cast<uint8_t>((a * 255UL) / half);
}

static inline uint8_t iconPix16(const uint8_t* frame, const int x, const int y) {
  return pgm_read_byte(frame + y * ICON_W + x);
}

static inline void drawIcon16x16Crossfade(const int x0, const int y0, const Icon16x16PalAnim* icon, const uint8_t alpha8, const CRGB& bg) {
  for (int y = 0; y < ICON_H; y++) {
    for (int x = 0; x < ICON_W; x++) {
      const uint8_t ia = iconPix16(icon->f0, x, y);
      const uint8_t ib = iconPix16(icon->f1, x, y);
      const CRGB ca = (ia == 0) ? bg : icon->pal[ia];
      const CRGB cb = (ib == 0) ? bg : icon->pal[ib];
      CRGB c = blend(ca, cb, alpha8);
      if (g_isNight) c = scaleColorPct(c, g_nightDimPct);
      if (c != bg) setPixel(x0 + x, y0 + y, c);
    }
    yield();
  }
}

// Font 3x5
const uint8_t font3x5_digits[10][5] = {
  {0b111,0b101,0b101,0b101,0b111},{0b010,0b110,0b010,0b010,0b111},{0b111,0b001,0b111,0b100,0b111},
  {0b111,0b001,0b111,0b001,0b111},{0b101,0b101,0b111,0b001,0b001},{0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111},{0b111,0b001,0b001,0b001,0b001},{0b111,0b101,0b111,0b101,0b111},
  {0b111,0b101,0b111,0b001,0b111}
};
const uint8_t glyph_minus[5] = {0b000,0b000,0b111,0b000,0b000};
const uint8_t glyph_pct[5] = {0b101,0b001,0b010,0b100,0b101};

static inline void drawGlyph3x5(const int x, const int y, const uint8_t g[5], const CRGB& col) {
  for (int ry = 0; ry < 5; ry++) {
    const uint8_t row = g[ry];
    for (int rx = 0; rx < 3; rx++) if (row & (1 << (2 - rx))) setPixel(x + rx, y + ry, col);
  }
}

static inline int drawChar3x5(const int x, const int y, const char ch, const CRGB& col) {
  if (ch >= '0' && ch <= '9') { drawGlyph3x5(x, y, font3x5_digits[ch - '0'], col); return 4; }
  if (ch == '-') { drawGlyph3x5(x, y, glyph_minus, col); return 4; }
  if (ch == '%') { drawGlyph3x5(x, y, glyph_pct, col); return 4; }
  return 4;
}

static inline int textWidth3x5(const char* s) { const int n = static_cast<int>(strlen(s)); return n ? (n * 4 - 1) : 0; }

static inline void drawText3x5(int x, const int y, const char* s, const CRGB& col) {
  for (int i = 0; s[i]; i++) x += drawChar3x5(x, y, s[i], col);
}

static inline void drawTextRight3x5(const int rightEdgeX, const int y, const char* s, const CRGB& col) {
  int x = rightEdgeX - textWidth3x5(s) + 1;
  if (x < 0) x = 0;
  drawText3x5(x, y, s, col);
}

static inline void drawDegreeDot2x2(const int x, const int y, const CRGB& col) {
  setPixel(x, y, col); setPixel(x + 1, y, col); setPixel(x, y + 1, col); setPixel(x + 1, y + 1, col);
}

const Icon16x16PalAnim* pickIcon() {
  const bool night = (g_icon[2] == 'n');
  if (g_icon[0] == '0' && g_icon[1] == '1') return night ? &ICON_NIGHT_CLEAR : &ICON_DAY_CLEAR;
  if ((g_icon[0] == '0' && (g_icon[1] == '9')) || (g_icon[0] == '1' && g_icon[1] == '0')) return night ? &ICON_NIGHT_RAIN : &ICON_DAY_RAIN;
  return night ? &ICON_NIGHT_CLOUD : &ICON_DAY_CLOUD;
}

String stateJson() {
  bool wifi_ok = (WiFi.status() == WL_CONNECTED);

  char ipbuf[20] = "";
  if (wifi_ok) {
    IPAddress ip = WiFi.localIP();
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }

  char dt[32] = "";
  if (timeIsValid()) {
    time_t now = time(nullptr);
    struct tm t{};
    localtime_r(&now, &t);
    snprintf(dt, sizeof(dt), "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  }

  const int tempInt = (!isnan(g_tempC)) ? static_cast<int>(lroundf(g_tempC)) : 99999;
  const int hum = (g_humidity >= 0) ? g_humidity : -1;
  char ctHex[8], chHex[8], ds[6], ns[6];
  toHexColorBuf(g_colTemp, ctHex); toHexColorBuf(g_colHum, chHex);
  minToHHMMBuf(g_dayStartMin, ds); minToHHMMBuf(g_nightStartMin, ns);

  char rssiBuf[12] = "null";
  if (wifi_ok) snprintf(rssiBuf, sizeof(rssiBuf), "%d", WiFi.RSSI());

  char tempPart[24], humPart[24];
  if (tempInt == 99999) snprintf(tempPart, sizeof(tempPart), "null"); else snprintf(tempPart, sizeof(tempPart), "%d", tempInt);
  if (hum < 0) snprintf(humPart, sizeof(humPart), "null"); else snprintf(humPart, sizeof(humPart), "%d", hum);

  static char json[560];
  snprintf(json, sizeof(json),
           "{"
           "\"wifi_ok\":%s,\"ip\":\"%s\",\"rssi_dbm\":%s,\"datetime\":\"%s\","
           "\"mode\":\"%s\",\"preview_night\":%s,\"br_pct\":%u,\"ct\":\"%s\",\"ch\":\"%s\","
           "\"ds\":\"%s\",\"ns\":\"%s\",\"owm\":%u,\"arh\":%u,\"nbp\":%u,\"nbm\":%u,\"ndp\":%u,\"asp\":%u,"
           "\"meteo_ok\":%s,\"icon\":\"%s\",\"temp_c\":%s,\"hum\":%s"
           "}",
           wifi_ok ? "true" : "false", ipbuf, rssiBuf, dt,
           g_isNight ? "NOTTE" : "GIORNO", g_forceNightPreview ? "true" : "false",
           brPctFrom255(g_brightness), ctHex, chHex, ds, ns,
           g_owIntervalMin, g_autoResetHours, g_nightBrightnessPct, g_nightBrightnessMin, g_nightDimPct, g_animSpeedPct,
           g_meteoOk ? "true" : "false", g_icon, tempPart, humPart);

  lastStateServeMs = millis();
  return String(json);
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Meteo 16x32</title>
<style>body{font-family:system-ui;background:#0b0d12;color:#e8eef6;margin:0;padding:16px}.card{max-width:900px;margin:auto;background:#141a22;border-radius:14px;padding:14px}.row{margin:10px 0}.mono{font-family:monospace}.btn{padding:8px 12px;border-radius:8px;border:0;background:#2c5bd8;color:white;cursor:pointer}</style>
</head><body><div class="card"><h3>Meteo 16x32</h3>
<div class="row">WiFi: <span id="wifi">...</span> | Meteo: <span id="meteo">...</span></div>
<div class="row">Luminosità giorno <input id="br" type="range" min="1" max="100"><span id="brv" class="mono"></span>%</div>
<div class="row">Notte % <input id="nbp" type="range" min="1" max="100"><span id="nbpv" class="mono"></span>%</div>
<div class="row">Min notte <input id="nbm" type="range" min="1" max="80"><span id="nbmv" class="mono"></span></div>
<div class="row">Dim notte % <input id="ndp" type="range" min="1" max="100"><span id="ndpv" class="mono"></span>%</div>
<div class="row">Anim % <input id="asp" type="range" min="1" max="100"><span id="aspv" class="mono"></span>%</div>
<div class="row">Temp <input id="ct" type="color"> Hum <input id="ch" type="color"></div>
<div class="row">Giorno <input id="ds" type="time"> Notte <input id="ns" type="time"></div>
<div class="row"><button class="btn" id="save">Salva</button> <button class="btn" id="pn">Preview NOTTE</button> <button class="btn" id="rst">Riavvia</button></div>
<div class="row mono" id="msg">—</div></div>
<script>
const $=id=>document.getElementById(id);
function setUI(s){$('wifi').textContent=s.wifi_ok?('OK '+s.ip):'OFF';$('meteo').textContent=`${s.icon} T=${s.temp_c??'--'} H=${s.hum??'--'}`;
$('br').value=s.br_pct;$('brv').textContent=s.br_pct;$('nbp').value=s.nbp;$('nbpv').textContent=s.nbp;$('nbm').value=s.nbm;$('nbmv').textContent=s.nbm;$('ndp').value=s.ndp;$('ndpv').textContent=s.ndp;$('asp').value=s.asp;$('aspv').textContent=s.asp;$('ct').value=s.ct;$('ch').value=s.ch;$('ds').value=s.ds;$('ns').value=s.ns;}
async function load(){const r=await fetch('/state');setUI(await r.json());}
async function save(){const p={br_pct:+$('br').value,nbp:+$('nbp').value,nbm:+$('nbm').value,ndp:+$('ndp').value,asp:+$('asp').value,ct:$('ct').value,ch:$('ch').value,ds:$('ds').value,ns:$('ns').value};const r=await fetch('/set',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});setUI(await r.json());$('msg').textContent='Salvato';}
$('save').onclick=save;$('pn').onclick=async()=>{const r=await fetch('/previewNight',{method:'POST'});setUI(await r.json());};$('rst').onclick=()=>fetch('/restart',{method:'POST'});
['br','nbp','nbm','ndp','asp'].forEach(id=>$(id).oninput=()=>$(id+'v').textContent=$(id).value);
load();setInterval(load,2500);
</script></body></html>)rawliteral";

static bool argOrJsonStr(const String& key, String& out) {
  if (server.hasArg(key)) { out = server.arg(key); return true; }
  const String body = server.arg("plain");
  if (body.length() < 2) return false;
  const String k = String("\"") + key + "\"";
  int i = body.indexOf(k); if (i < 0) return false;
  i = body.indexOf(':', i); if (i < 0) return false;
  i++;
  while (i < static_cast<int>(body.length()) && body[i] == ' ') i++;
  if (i >= static_cast<int>(body.length()) || body[i] != '"') return false;
  const int j = body.indexOf('"', i + 1);
  if (j < 0) return false;
  out = body.substring(i + 1, j);
  return true;
}

static bool argOrJsonInt(const String& key, int& out) {
  if (server.hasArg(key)) { out = server.arg(key).toInt(); return true; }
  const String body = server.arg("plain");
  if (body.length() < 2) return false;
  const String k = String("\"") + key + "\"";
  int i = body.indexOf(k); if (i < 0) return false;
  i = body.indexOf(':', i); if (i < 0) return false;
  i++;
  while (i < static_cast<int>(body.length()) && body[i] == ' ') i++;
  int j = i;
  while (j < static_cast<int>(body.length()) && (body[j] == '-' || isDigit(body[j]))) j++;
  if (j == i) return false;
  out = body.substring(i, j).toInt();
  return true;
}

void handleSet() {
  int v;
  String s;
  if (argOrJsonInt("br_pct", v)) g_brightness = br255FromPct(constrain(v, 1, 100));
  if (argOrJsonInt("nbp", v)) g_nightBrightnessPct = constrain(v, 1, 100);
  if (argOrJsonInt("nbm", v)) g_nightBrightnessMin = constrain(v, 1, 80);
  if (argOrJsonInt("ndp", v)) g_nightDimPct = constrain(v, 1, 100);
  if (argOrJsonInt("asp", v)) g_animSpeedPct = constrain(v, 1, 100);
  if (argOrJsonStr("ct", s)) g_colTemp = parseHexColor(s, g_colTemp);
  if (argOrJsonStr("ch", s)) g_colHum = parseHexColor(s, g_colHum);
  if (argOrJsonStr("ds", s)) g_dayStartMin = hhmmToMinSafe(s, g_dayStartMin);
  if (argOrJsonStr("ns", s)) g_nightStartMin = hhmmToMinSafe(s, g_nightStartMin);
  if (argOrJsonInt("owm", v)) g_owIntervalMin = constrain(v, 1, 120);
  if (argOrJsonInt("arh", v)) g_autoResetHours = constrain(v, 0, 168);

  savePrefs();
  updateNightFlag();
  g_dirty = true;
  server.send(200, "application/json", stateJson());
}

void handlePreviewNight() {
  if (!g_forceNightPreview) {
    g_forceNightPreview = true;
    g_forceNightUntilMs = millis() + static_cast<unsigned long>(FORCE_NIGHT_SECONDS_DEFAULT) * 1000UL;
    g_isNight = true;
  } else {
    g_forceNightPreview = false;
    g_forceNightUntilMs = 0;
    updateNightFlag_Auto();
  }
  g_dirty = true;
  server.send(200, "application/json", stateJson());
}

void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/state", HTTP_GET, []() { server.send(200, "application/json", stateJson()); });
  server.on("/set", HTTP_POST, handleSet);
  server.on("/previewNight", HTTP_POST, handlePreviewNight);
  server.on("/restart", HTTP_POST, []() { server.send(200, "text/plain", "OK"); delay(120); ESP.restart(); });
  server.begin();
}

void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { FastLED.clear(true); });
  ArduinoOTA.onEnd([]() { ESP.restart(); });
  ArduinoOTA.onError([](ota_error_t error) { (void)error; });
  ArduinoOTA.begin();
}

void wifiEnsureConnected() {
  if (WiFi.status() == WL_CONNECTED) { lastWifiOkMs = millis(); return; }
  if (millis() - lastWifiOkMs > WIFI_GRACE_MS) {
    WiFi.disconnect(true, true);
    delay(80);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastWifiOkMs = millis();
  }
}

void healthCheck() {
  if (millis() - lastHealthMs < HEALTH_PERIOD_MS) return;
  lastHealthMs = millis();
  wifiEnsureConnected();

  if (WiFi.status() == WL_CONNECTED && lastStateServeMs != 0 && (millis() - lastStateServeMs > 180000UL)) ESP.restart();
  if (HARD_RESTART_MS > 0 && (millis() - bootMs > HARD_RESTART_MS)) ESP.restart();
}

void handleAutoReset() {
  if (g_autoResetHours == 0) return;
  const unsigned long limitMs = static_cast<unsigned long>(g_autoResetHours) * 3600UL * 1000UL;
  if (millis() - bootMs >= limitMs) { delay(120); ESP.restart(); }
}

void renderIfNeeded() {
  updateNightFlag();

  const uint8_t alpha8 = computeAlpha8();
  const uint8_t effBr = effectiveBrightness();
  const int t = (g_meteoOk && !isnan(g_tempC)) ? static_cast<int>(lroundf(g_tempC)) : 99999;
  const int hum = (g_meteoOk && g_humidity >= 0) ? g_humidity : 9999;

  const bool changed = g_dirty || ((millis() - lastRenderMs >= MIN_FRAME_MS) && alpha8 != lastAlpha8) ||
                       (g_isNight != lastNight) || (strncmp(g_icon, lastIcon, 3) != 0) ||
                       (t != lastT) || (hum != lastHum) || (effBr != lastEffBr);
  if (!changed) return;

  lastRenderMs = millis();
  lastAlpha8 = alpha8;
  lastNight = g_isNight;
  memcpy(lastIcon, g_icon, 4);
  lastT = t;
  lastHum = hum;
  lastEffBr = effBr;
  g_dirty = false;

  CRGB cTmp = g_colTemp;
  CRGB cHum = g_colHum;
  if (g_isNight) { cTmp = scaleColorPct(cTmp, g_nightDimPct); cHum = scaleColorPct(cHum, g_nightDimPct); }

  fill_solid(leds, NUM_LEDS, g_colBg);

  constexpr int TEXT_W = 11;
  constexpr int RIGHT_EDGE = MATRIX_W - 1;
  const int textX0 = MATRIX_W - TEXT_W;

  int iconX = (textX0 - 16) / 2;
  if (iconX < 0) iconX = 0;

  drawIcon16x16Crossfade(iconX, 1, pickIcon(), alpha8, g_colBg);

  char ts[8];
  if (t == 99999) strcpy(ts, "--"); else snprintf(ts, sizeof(ts), "%d", t);
  int tw = textWidth3x5(ts);
  int tx = RIGHT_EDGE - tw - 3;
  if (tx < textX0) tx = textX0;
  drawText3x5(tx, 1, ts, cTmp);
  drawDegreeDot2x2(RIGHT_EDGE - 1, 1, cTmp);

  char hs[8];
  if (hum != 9999) snprintf(hs, sizeof(hs), "%d%%", hum); else strcpy(hs, "--%");
  drawTextRight3x5(RIGHT_EDGE, 10, hs, cHum);

  FastLED.setBrightness(effBr);
  FastLED.show();
}

void setup() {
  delay(120);
  bootMs = millis();

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 10000, .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, .trigger_panic = true };
  esp_task_wdt_init(&wdt_cfg);
  esp_task_wdt_add(NULL);
#else
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
#endif

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
  FastLED.setBrightness(8);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  loadPrefs();
  g_dirty = true;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    esp_task_wdt_reset();
  }
  lastWifiOkMs = millis();

  if (WiFi.status() == WL_CONNECTED) setupOTA();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  for (int i = 0; i < 30; i++) {
    if (timeIsValid()) break;
    delay(200);
    esp_task_wdt_reset();
  }

  setupWeb();
  g_meteoOk = fetchMeteo();
  meteoLastMs = millis();
  updateNightFlag();
  g_dirty = true;
}

void loop() {
  esp_task_wdt_reset();
  ArduinoOTA.handle();
  server.handleClient();

  const unsigned long intervalMs = static_cast<unsigned long>(g_owIntervalMin) * 60UL * 1000UL;
  if (millis() - meteoLastMs >= intervalMs) {
    meteoLastMs = millis();
    g_meteoOk = fetchMeteo();
    g_dirty = true;
  }

  healthCheck();
  handleAutoReset();
  renderIfNeeded();
  delay(5);
}
