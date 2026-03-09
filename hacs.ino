#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <time.h>

#define PIN_LED 13
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define MATRIX_FLAGS (NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG)

const char *WIFI_SSID = "GieMa";
const char *WIFI_PASSWORD = "ernestino";
const char *WALLET = "bc1que63wfl066wx9d3mq0fjz9tr7pqslg0aww4h39";

const char *POOL_URLS[] = {
    "https://eusolostats.ckpool.org/",
    "https://eusolostats.ckpool.org/users/",
    "https://solo.ckpool.org/users/"};

const uint8_t SCREEN_COUNT = 6;
const uint32_t FETCH_INTERVAL_MS = 25000;

Adafruit_NeoMatrix matrix(MATRIX_WIDTH, MATRIX_HEIGHT, PIN_LED, MATRIX_FLAGS, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

struct Metric {
  const char *key;
  const char *label;
  float value;
};

Metric metrics[SCREEN_COUNT] = {
    {"5m", "5M", 0},
    {"1h", "1H", 0},
    {"1d", "1D", 0},
    {"7d", "7D", 0},
    {"total share", "TS", 0},
    {"best share", "BS", 0}};

struct ScreenColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct AppSettings {
  uint16_t rotateMs = 3500;
  uint8_t brightnessDay = 40;
  uint8_t brightnessNight = 8;
  bool nightEnabled = false;
  uint8_t nightStartHour = 23;
  uint8_t nightEndHour = 7;
  bool autoResetEnabled = false;
  uint16_t autoResetHours = 24;
  uint8_t fontYOffset = 0;   // -1..1 encoded as 0..2 in UI
  uint8_t charSpacing = 5;   // 5=fit 6 chars in 32px
  ScreenColor colors[SCREEN_COUNT] = {{0, 200, 255}, {255, 100, 0}, {0, 255, 120}, {255, 255, 60}, {240, 60, 255}, {255, 255, 255}};
};

AppSettings settings;
uint8_t currentScreen = 0;
unsigned long lastRotateMs = 0;
unsigned long lastFetchMs = 0;
unsigned long bootMs = 0;
String lastError = "";

String lowerCopy(String s) {
  s.toLowerCase();
  return s;
}

float parseNumber(const String &txt) {
  String out = "";
  for (size_t i = 0; i < txt.length(); i++) {
    char c = txt[i];
    if ((c >= '0' && c <= '9') || c == '.') out += c;
    else if (c == ',') out += '.';
    else if (out.length()) break;
  }
  return out.length() ? out.toFloat() : 0.0f;
}

float toGFactor(const String &unitRaw) {
  String unit = lowerCopy(unitRaw);
  if (unit.indexOf("ph") >= 0 || unit.indexOf("ps") >= 0) return 1000000.0f;
  if (unit.indexOf("th") >= 0 || unit.indexOf("ts") >= 0) return 1000.0f;
  if (unit.indexOf("gh") >= 0 || unit.indexOf("gs") >= 0) return 1.0f;
  if (unit.indexOf("mh") >= 0 || unit.indexOf("ms") >= 0) return 0.001f;
  if (unit.indexOf("kh") >= 0 || unit.indexOf("ks") >= 0) return 0.000001f;
  if (unit.indexOf(" h") >= 0 || unit == "h") return 0.000000001f;

  if (unit.indexOf("p") >= 0) return 1000000.0f;
  if (unit.indexOf("t") >= 0) return 1000.0f;
  if (unit.indexOf("g") >= 0) return 1.0f;
  if (unit.indexOf("m") >= 0) return 0.001f;
  if (unit.indexOf("k") >= 0) return 0.000001f;

  return 1.0f;
}

float extractMetric(const String &html, const String &key) {
  String low = lowerCopy(html);
  int p = low.indexOf(lowerCopy(key));
  if (p < 0) return 0;

  String area = html.substring(p, min((int)html.length(), p + 180));
  int startNum = -1;
  for (int i = 0; i < (int)area.length(); i++) {
    if (isDigit(area[i])) {
      startNum = i;
      break;
    }
  }
  if (startNum < 0) return 0;

  String n = area.substring(startNum);
  float value = parseNumber(n);

  String unit = "";
  bool numDone = false;
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    bool numeric = isDigit(c) || c == '.' || c == ',';
    if (!numDone && numeric) continue;
    numDone = true;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '/') {
      unit += c;
    } else if (unit.length()) {
      break;
    }
  }

  return value * toGFactor(unit);
}

String fetchPoolData() {
  WiFiClientSecure client;
  client.setInsecure();

  for (const char *base : POOL_URLS) {
    HTTPClient http;
    String url = String(base) + String(WALLET);
    if (!http.begin(client, url)) continue;

    http.setTimeout(14000);
    http.addHeader("User-Agent", "Mozilla/5.0 (ESP32 CKPool Monitor)");
    http.addHeader("Accept", "text/html,application/json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      http.end();
      lastError = "";
      return body;
    }
    lastError = "HTTP " + String(code) + " " + url;
    http.end();
  }

  return "";
}

void updateMetrics() {
  String body = fetchPoolData();
  if (!body.length()) return;

  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    metrics[i].value = extractMetric(body, metrics[i].key);
  }
}

String pad4(float v) {
  int n = (int)round(v);
  if (n < 0) n = 0;
  if (n > 9999) n = 9999;
  char out[5];
  snprintf(out, sizeof(out), "%04d", n);
  return String(out);
}

uint8_t getBrightness() {
  if (!settings.nightEnabled) return settings.brightnessDay;

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 100)) return settings.brightnessDay;
  int h = timeInfo.tm_hour;

  if (settings.nightStartHour < settings.nightEndHour) {
    return (h >= settings.nightStartHour && h < settings.nightEndHour) ? settings.brightnessNight : settings.brightnessDay;
  }
  return (h >= settings.nightStartHour || h < settings.nightEndHour) ? settings.brightnessNight : settings.brightnessDay;
}

void drawCondensedText(const String &text, uint16_t color, int y) {
  int x = 0;
  for (size_t i = 0; i < text.length(); i++) {
    matrix.drawChar(x, y, text[i], color, 0, 1);
    x += settings.charSpacing;
  }
}

void drawScreen(uint8_t idx) {
  matrix.fillScreen(0);
  matrix.setBrightness(getBrightness());
  ScreenColor c = settings.colors[idx];
  uint16_t color = matrix.Color(c.r, c.g, c.b);

  String text = String(metrics[idx].label) + pad4(metrics[idx].value);
  int y = constrain(settings.fontYOffset - 1, -1, 1);
  drawCondensedText(text, color, y);

  matrix.show();
}

void saveSettings() {
  prefs.putUShort("rot", settings.rotateMs);
  prefs.putUChar("bday", settings.brightnessDay);
  prefs.putUChar("bnight", settings.brightnessNight);
  prefs.putBool("night", settings.nightEnabled);
  prefs.putUChar("nsh", settings.nightStartHour);
  prefs.putUChar("neh", settings.nightEndHour);
  prefs.putBool("arst", settings.autoResetEnabled);
  prefs.putUShort("arh", settings.autoResetHours);
  prefs.putUChar("fy", settings.fontYOffset);
  prefs.putUChar("sp", settings.charSpacing);

  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    String p = "c" + String(i);
    uint32_t rgb = ((uint32_t)settings.colors[i].r << 16) | ((uint32_t)settings.colors[i].g << 8) | settings.colors[i].b;
    prefs.putUInt(p.c_str(), rgb);
  }
}

void loadSettings() {
  settings.rotateMs = prefs.getUShort("rot", settings.rotateMs);
  settings.brightnessDay = prefs.getUChar("bday", settings.brightnessDay);
  settings.brightnessNight = prefs.getUChar("bnight", settings.brightnessNight);
  settings.nightEnabled = prefs.getBool("night", settings.nightEnabled);
  settings.nightStartHour = prefs.getUChar("nsh", settings.nightStartHour);
  settings.nightEndHour = prefs.getUChar("neh", settings.nightEndHour);
  settings.autoResetEnabled = prefs.getBool("arst", settings.autoResetEnabled);
  settings.autoResetHours = prefs.getUShort("arh", settings.autoResetHours);
  settings.fontYOffset = prefs.getUChar("fy", settings.fontYOffset);
  settings.charSpacing = prefs.getUChar("sp", settings.charSpacing);

  settings.rotateMs = constrain(settings.rotateMs, 500, 30000);
  settings.brightnessDay = constrain(settings.brightnessDay, 1, 255);
  settings.brightnessNight = constrain(settings.brightnessNight, 1, 255);
  settings.nightStartHour = constrain(settings.nightStartHour, 0, 23);
  settings.nightEndHour = constrain(settings.nightEndHour, 0, 23);
  settings.autoResetHours = constrain(settings.autoResetHours, 1, 168);
  settings.fontYOffset = constrain(settings.fontYOffset, 0, 2);
  settings.charSpacing = constrain(settings.charSpacing, 4, 5);

  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    String p = "c" + String(i);
    uint32_t rgb = prefs.getUInt(p.c_str(), ((uint32_t)settings.colors[i].r << 16) | ((uint32_t)settings.colors[i].g << 8) | settings.colors[i].b);
    settings.colors[i].r = (rgb >> 16) & 0xFF;
    settings.colors[i].g = (rgb >> 8) & 0xFF;
    settings.colors[i].b = rgb & 0xFF;
  }
}

int jsonInt(const String &b, const char *key, int fallback) {
  String k = String("\"") + key + "\":";
  int p = b.indexOf(k);
  if (p < 0) return fallback;
  int s = p + k.length();
  while (s < (int)b.length() && (b[s] == ' ' || b[s] == '"')) s++;
  int e = s;
  while (e < (int)b.length() && (isDigit(b[e]) || b[e] == '-')) e++;
  return b.substring(s, e).toInt();
}

bool jsonBool(const String &b, const char *key, bool fallback) {
  String k = String("\"") + key + "\":";
  int p = b.indexOf(k);
  if (p < 0) return fallback;
  int s = p + k.length();
  while (s < (int)b.length() && (b[s] == ' ' || b[s] == '"')) s++;
  if (b.substring(s, s + 4) == "true") return true;
  if (b.substring(s, s + 5) == "false") return false;
  return fallback;
}

String statusJson() {
  String j = "{";
  j += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"error\":\"" + lastError + "\",";
  j += "\"values\":{";
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    j += "\"" + String(metrics[i].label) + "\":" + String(metrics[i].value, 3);
    if (i < SCREEN_COUNT - 1) j += ",";
  }
  j += "},\"settings\":{";
  j += "\"rotateMs\":" + String(settings.rotateMs) + ",";
  j += "\"brightnessDay\":" + String(settings.brightnessDay) + ",";
  j += "\"brightnessNight\":" + String(settings.brightnessNight) + ",";
  j += "\"nightEnabled\":" + String(settings.nightEnabled ? "true" : "false") + ",";
  j += "\"nightStartHour\":" + String(settings.nightStartHour) + ",";
  j += "\"nightEndHour\":" + String(settings.nightEndHour) + ",";
  j += "\"autoResetEnabled\":" + String(settings.autoResetEnabled ? "true" : "false") + ",";
  j += "\"autoResetHours\":" + String(settings.autoResetHours) + ",";
  j += "\"fontYOffset\":" + String(settings.fontYOffset) + ",";
  j += "\"charSpacing\":" + String(settings.charSpacing) + ",";
  j += "\"colors\":[";
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    j += "{";
    j += "\"r\":" + String(settings.colors[i].r) + ",";
    j += "\"g\":" + String(settings.colors[i].g) + ",";
    j += "\"b\":" + String(settings.colors[i].b);
    j += "}";
    if (i < SCREEN_COUNT - 1) j += ",";
  }
  j += "]}}";
  return j;
}

void setupWeb() {
  server.on("/", HTTP_GET, []() {
    String page = R"HTML(
<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>CKPool Monitor</title>
<style>
:root{--bg:#0a0f1a;--card:#121a2b;--txt:#e8eeff;--muted:#8ea2d9;--accent:#5fa8ff;--ok:#2cd486}
*{box-sizing:border-box} body{margin:0;background:radial-gradient(circle at top,#16203a,var(--bg));color:var(--txt);font:14px/1.45 Inter,Segoe UI,Arial,sans-serif}
.wrap{max-width:980px;margin:20px auto;padding:12px} h1{font-size:24px;margin:8px 0 16px} h2{font-size:16px;margin:0 0 12px}
.card{background:linear-gradient(180deg,#16213a,var(--card));border:1px solid #243455;border-radius:14px;padding:14px;margin-bottom:12px;box-shadow:0 12px 30px rgba(0,0,0,.25)}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px} .grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.kpi{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:8px} .pill{background:#0f1526;border:1px solid #2b3d64;border-radius:10px;padding:8px;text-align:center}
label{display:block;font-size:12px;color:var(--muted);margin:0 0 4px} input,select,button{width:100%;background:#0f1526;color:var(--txt);border:1px solid #30466f;border-radius:8px;padding:8px}
button{background:linear-gradient(180deg,#5fa8ff,#3475d4);border:none;font-weight:700;cursor:pointer} .btn-danger{background:linear-gradient(180deg,#ff7b86,#d65060)}
small{color:var(--muted)} .ok{color:var(--ok)} .err{color:#ff9aa8}
@media (max-width:860px){.kpi{grid-template-columns:repeat(3,minmax(0,1fr))}.grid{grid-template-columns:1fr}}
</style></head><body><div class='wrap'>
<h1>⚡ CKPool Monitor ESP32</h1>
<div class='card'><h2>Stato</h2><div id='status'>Caricamento...</div></div>
<div class='card'><h2>Dati pool (GH/s - G share)</h2><div id='kpi' class='kpi'></div></div>
<div class='card'><h2>Colori per schermata (5M 1H 1D 7D TS BS)</h2><div class='grid3' id='colors'></div></div>
<div class='card'><h2>Display e sistema</h2>
<div class='grid'>
<div><label>Velocità cambio schermata (ms)</label><input id='rotateMs' type='number' min='500' max='30000'></div>
<div><label>Spaziatura caratteri (4-5)</label><input id='charSpacing' type='number' min='4' max='5'></div>
<div><label>Offset verticale (0=alto,1=centro,2=basso)</label><input id='fontYOffset' type='number' min='0' max='2'></div>
<div><label>Luminosità giorno</label><input id='brightnessDay' type='number' min='1' max='255'></div>
<div><label>Luminosità notte</label><input id='brightnessNight' type='number' min='1' max='255'></div>
<div><label>Modo notte</label><select id='nightEnabled'><option value='false'>OFF</option><option value='true'>ON</option></select></div>
<div><label>Inizio notte</label><input id='nightStartHour' type='number' min='0' max='23'></div>
<div><label>Fine notte</label><input id='nightEndHour' type='number' min='0' max='23'></div>
<div><label>Auto reset</label><select id='autoResetEnabled'><option value='false'>OFF</option><option value='true'>ON</option></select></div>
<div><label>Auto reset (ore)</label><input id='autoResetHours' type='number' min='1' max='168'></div>
</div>
<p style='display:flex;gap:8px;margin-top:12px'><button onclick='save()'>Salva impostazioni</button><button class='btn-danger' onclick='restart()'>Reset ESP32</button></p>
<small>Le modifiche vengono salvate in memoria persistente (NVS).</small>
</div>
</div>
<script>
const labels=['5M','1H','1D','7D','TS','BS'];
function cInput(i,ch){return `<div><label>${labels[i]} ${ch}</label><input id='c${i}${ch}' type='number' min='0' max='255'></div>`}
document.getElementById('colors').innerHTML=[0,1,2,3,4,5].map(i=>cInput(i,'r')+cInput(i,'g')+cInput(i,'b')).join('');
async function load(){
  const r=await fetch('/api/status'); const j=await r.json();
  const ok=j.connected?"<span class='ok'>Connesso</span>":"<span class='err'>Disconnesso</span>";
  document.getElementById('status').innerHTML=`WiFi: ${ok} | IP: ${j.ip} ${j.error?`| <span class='err'>${j.error}</span>`:''}`;
  document.getElementById('kpi').innerHTML=labels.map(k=>`<div class='pill'><small>${k}</small><div style='font-size:20px;font-weight:700'>${Number(j.values[k]).toFixed(3)}</div></div>`).join('');
  Object.entries(j.settings).forEach(([k,v])=>{const e=document.getElementById(k);if(e)e.value=String(v)});
  j.settings.colors.forEach((c,i)=>{['r','g','b'].forEach(ch=>{document.getElementById(`c${i}${ch}`).value=c[ch];})});
}
async function save(){
  const d={};
  ['rotateMs','brightnessDay','brightnessNight','nightEnabled','nightStartHour','nightEndHour','autoResetEnabled','autoResetHours','fontYOffset','charSpacing']
    .forEach(k=>d[k]=document.getElementById(k).value);
  labels.forEach((_,i)=>{ d[`c${i}r`]=document.getElementById(`c${i}r`).value; d[`c${i}g`]=document.getElementById(`c${i}g`).value; d[`c${i}b`]=document.getElementById(`c${i}b`).value; });
  await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});
  await load();
}
async function restart(){ await fetch('/api/restart',{method:'POST'}); }
setInterval(load,3000); load();
</script></body></html>)HTML";
    server.send(200, "text/html", page);
  });

  server.on("/api/status", HTTP_GET, []() { server.send(200, "application/json", statusJson()); });

  server.on("/api/restart", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
  });

  server.on("/api/settings", HTTP_POST, []() {
    String b = server.arg("plain");
    settings.rotateMs = constrain(jsonInt(b, "rotateMs", settings.rotateMs), 500, 30000);
    settings.brightnessDay = constrain(jsonInt(b, "brightnessDay", settings.brightnessDay), 1, 255);
    settings.brightnessNight = constrain(jsonInt(b, "brightnessNight", settings.brightnessNight), 1, 255);
    settings.nightEnabled = jsonBool(b, "nightEnabled", settings.nightEnabled);
    settings.nightStartHour = constrain(jsonInt(b, "nightStartHour", settings.nightStartHour), 0, 23);
    settings.nightEndHour = constrain(jsonInt(b, "nightEndHour", settings.nightEndHour), 0, 23);
    settings.autoResetEnabled = jsonBool(b, "autoResetEnabled", settings.autoResetEnabled);
    settings.autoResetHours = constrain(jsonInt(b, "autoResetHours", settings.autoResetHours), 1, 168);
    settings.fontYOffset = constrain(jsonInt(b, "fontYOffset", settings.fontYOffset), 0, 2);
    settings.charSpacing = constrain(jsonInt(b, "charSpacing", settings.charSpacing), 4, 5);

    for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
      settings.colors[i].r = constrain(jsonInt(b, ("c" + String(i) + "r").c_str(), settings.colors[i].r), 0, 255);
      settings.colors[i].g = constrain(jsonInt(b, ("c" + String(i) + "g").c_str(), settings.colors[i].g), 0, 255);
      settings.colors[i].b = constrain(jsonInt(b, ("c" + String(i) + "b").c_str(), settings.colors[i].b), 0, 255);
    }

    saveSettings();
    drawScreen(currentScreen);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  prefs.begin("hacs", false);
  loadSettings();

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(settings.brightnessDay);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");

  setupWeb();
  bootMs = millis();
  updateMetrics();
  drawScreen(currentScreen);
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastFetchMs >= FETCH_INTERVAL_MS) {
    lastFetchMs = now;
    if (WiFi.status() == WL_CONNECTED) updateMetrics();
    drawScreen(currentScreen);
  }

  if (now - lastRotateMs >= settings.rotateMs) {
    lastRotateMs = now;
    currentScreen = (currentScreen + 1) % SCREEN_COUNT;
    drawScreen(currentScreen);
  }

  if (settings.autoResetEnabled) {
    unsigned long interval = (unsigned long)settings.autoResetHours * 3600000UL;
    if (interval && now - bootMs >= interval) ESP.restart();
  }
}
