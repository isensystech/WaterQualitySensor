#include "shared.h"
#include "portal_page.h"    // big HTML raw string lives here so the .ino preprocessor can't truncate it

// version string lives in shared.h; this fallback keeps the file compiling if that edit is missed
#ifndef FW_VERSION
#define FW_VERSION "0.7.0"
#endif

static WebServer  server(80);
static DNSServer  dns;
static IPAddress  apIP(192, 168, 4, 1);
static bool       s_active = false;

static const char *THKEY[M_COUNT] = {"temp", "ph", "orp", "ec", "sal", "depth", "cyc"};

bool portalActive() { return s_active; }

// ---------------- time persistence (best-effort until RTC revision) ----------------
void setUnixFromMillisEpoch(uint64_t epoch_ms) {
  g_epoch0 = (uint32_t)(epoch_ms / 1000ULL);
  g_epochAtMillis = millis();
  g_timeSynced = true;
  g_timeApprox = false;          // freshly synced from a phone == trusted
  stateSave();
}

static void writeThresh(JsonObject root) {                 // omit NAN bounds -> cleaner json
  JsonObject th = root["thresh"].to<JsonObject>();
  for (int m = 0; m < M_COUNT; m++) {
    JsonObject o = th[THKEY[m]].to<JsonObject>();
    Threshold &t = deploy.thresh[m];
    if (!isnan(t.warnLo))  o["wlo"] = t.warnLo;
    if (!isnan(t.warnHi))  o["whi"] = t.warnHi;
    if (!isnan(t.alarmLo)) o["alo"] = t.alarmLo;
    if (!isnan(t.alarmHi)) o["ahi"] = t.alarmHi;
  }
}

static void readThresh(JsonVariant th) {                   // full source of truth: missing -> disabled
  if (th.isNull()) return;
  for (int m = 0; m < M_COUNT; m++) {
    JsonVariant o = th[THKEY[m]];
    deploy.thresh[m].warnLo  = o["wlo"] | (float)NAN;
    deploy.thresh[m].warnHi  = o["whi"] | (float)NAN;
    deploy.thresh[m].alarmLo = o["alo"] | (float)NAN;
    deploy.thresh[m].alarmHi = o["ahi"] | (float)NAN;
  }
}

static void cpy(char *dst, size_t n, const char *src) { strncpy(dst, src ? src : "", n - 1); dst[n - 1] = 0; }

void stateSave() {
  if (!sdEnsure()) { Serial.println("state.json: SD not ready, skipping save"); return; }
  JsonDocument d;
  d["epoch"]   = g_epoch0 ? (g_epoch0 + (millis() - g_epochAtMillis) / 1000UL) : 0;
  d["cast"]    = deploy.castNum;
  d["accent"]  = deploy.accent;
  d["cyc_en"]  = deploy.cyc_en;
  d["cyc_u"]   = deploy.cyc_units;
  d["cyc_s"]   = deploy.cyc_std;
  d["dim"]     = deploy.dimMinutes;
  // operator metadata persists so the NEXT mission can prefill from the last one
  d["mission"] = deploy.mission;
  d["op"]      = deploy.op;
  d["site"]    = deploy.site;
  d["wt"]      = deploy.waterType;
  d["notes"]   = deploy.notes;
  if (deploy.hasPos) { d["lat"] = deploy.lat; d["lon"] = deploy.lon; }
  writeThresh(d.as<JsonObject>());
  if (SD.exists("/state.json")) SD.remove("/state.json");
  File f = SD.open("/state.json", FILE_WRITE); if (!f) return;
  serializeJson(d, f); f.close();
}

void stateLoad() {
  if (!SD.exists("/state.json")) return;
  File f = SD.open("/state.json", FILE_READ); if (!f) return;
  JsonDocument d; if (deserializeJson(d, f)) { f.close(); return; } f.close();
  uint32_t e = d["epoch"] | 0UL;
  deploy.castNum = d["cast"]   | 0UL;
  deploy.accent  = d["accent"] | deploy.accent;
  deploy.cyc_en  = d["cyc_en"] | deploy.cyc_en;
  if (d["cyc_u"].is<const char *>()) cpy(deploy.cyc_units, sizeof(deploy.cyc_units), d["cyc_u"]);
  deploy.cyc_std = d["cyc_s"] | deploy.cyc_std;
  deploy.dimMinutes = d["dim"] | deploy.dimMinutes;   // absent -> keep default (clamped on use)
  // last-mission metadata (guarded so an absent key never self-copies)
  if (d["mission"].is<const char *>()) cpy(deploy.mission,   sizeof(deploy.mission),   d["mission"]);
  if (d["op"].is<const char *>())      cpy(deploy.op,        sizeof(deploy.op),        d["op"]);
  if (d["site"].is<const char *>())    cpy(deploy.site,      sizeof(deploy.site),      d["site"]);
  if (d["wt"].is<const char *>())      cpy(deploy.waterType, sizeof(deploy.waterType), d["wt"]);
  if (d["notes"].is<const char *>())   cpy(deploy.notes,     sizeof(deploy.notes),     d["notes"]);
  if (d["lat"].is<double>() && d["lon"].is<double>()) {
    deploy.lat = d["lat"].as<double>(); deploy.lon = d["lon"].as<double>(); deploy.hasPos = true;
  }
  readThresh(d["thresh"]);     // override boot defaults with the last-saved mission bands
  if (e) {            // we have a stored wall-clock, but the power-off gap is unknown
    g_epoch0 = e; g_epochAtMillis = millis();
    g_timeSynced = true; g_timeApprox = true;   // flag as approximate until re-synced / RTC
    Serial.println("state.json: time loaded (APPROX - off-time gap unknown)");
  }
}

static void handleRoot()  { server.send_P(200, "text/html", PAGE); }
// Answer OS connectivity probes as "online" so the phone keeps the WiFi connected and does
// NOT launch the captive mini-browser (which closes on every keystroke and then blacklists
// the network, causing the "unable to join" loop). Configure in a normal browser tab instead.
static void handle204()   { server.send(204, "text/plain", ""); }
static void handleApple() { server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); }
static void handleMsTxt() { server.send(200, "text/plain", "Microsoft Connect Test"); }

static void handleSync() {
  JsonDocument d;
  if (!deserializeJson(d, server.arg("plain"))) {
    uint64_t ms = d["epoch_ms"] | 0ULL;
    if (ms > 1000000000000ULL) { setUnixFromMillisEpoch(ms); server.send(200, "text/plain", "ok"); return; }
  }
  server.send(400, "text/plain", "bad");
}

// GET current settings so the form can prefill (NAN bounds serialize away -> blank inputs)
static void handleState() {
  JsonDocument d;
  JsonObject root = d.to<JsonObject>();
  root["ver"]     = FW_VERSION;
  root["synced"]  = g_timeSynced;
  root["approx"]  = g_timeApprox;
  root["mission"] = deploy.mission;
  root["op"]      = deploy.op;
  root["site"]    = deploy.site;
  root["wt"]      = deploy.waterType;
  root["notes"]   = deploy.notes;
  root["wx"]      = deploy.weather;
  root["accent"]  = deploy.accent;
  root["cyc_en"]  = deploy.cyc_en;
  root["cyc_u"]   = deploy.cyc_units;
  root["cyc_s"]   = deploy.cyc_std;
  root["dim"]     = dimMinutesEffective();   // clamped value so the slider shows the real default
  char gps[40] = "";
  if (deploy.hasPos) snprintf(gps, sizeof(gps), "%.5f,%.5f", deploy.lat, deploy.lon);
  root["gps"]     = gps;
  writeThresh(root);
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleDeploy() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad"); return; }
  cpy(deploy.mission, sizeof(deploy.mission), d["mission"] | "");
  cpy(deploy.op,      sizeof(deploy.op),      d["op"]      | "");
  cpy(deploy.site,    sizeof(deploy.site),    d["site"]    | "");
  cpy(deploy.waterType, sizeof(deploy.waterType), d["wt"]  | "");
  cpy(deploy.weather, sizeof(deploy.weather), d["wx"]      | "");
  cpy(deploy.notes,   sizeof(deploy.notes),   d["notes"]   | "");
  const char *gps = d["gps"] | "";
  deploy.hasPos = false;
  if (gps && strchr(gps, ',')) { deploy.lat = atof(gps); deploy.lon = atof(strchr(gps, ',') + 1); deploy.hasPos = (deploy.lat != 0 || deploy.lon != 0); }
  deploy.hasWeather = (strlen(deploy.weather) > 0);
  if (deploy.hasWeather) deploy.airTemp = atof(deploy.weather);
  deploy.accent = (uint8_t)(d["accent"] | (int)deploy.accent);
  if (deploy.accent > 3) deploy.accent = 0;
  deploy.cyc_en = d["cyc_en"] | deploy.cyc_en;
  cpy(deploy.cyc_units, sizeof(deploy.cyc_units), d["cyc_u"] | "");
  if (d["cyc_s"].is<float>()) deploy.cyc_std = d["cyc_s"].as<float>();   // NaN-safe: only overwrite if present/numeric
  int dm = d["dim"] | (int)dimMinutesEffective();                       // screen-dim timeout (min)
  if (dm < DIM_MIN_MIN) dm = DIM_MIN_MIN; else if (dm > DIM_MIN_MAX) dm = DIM_MIN_MAX;
  deploy.dimMinutes = (uint16_t)dm;
  g_accent = accentColor(deploy.accent);        // apply immediately so the run screen retints
  readThresh(d["thresh"]);     // present -> defines bands (blank fields = disabled)
  deploy.set = true;
  g_uiDirty = true;                          // ask the run screen to retint on the next loop
  server.send(200, "text/plain", "ok");      // respond BEFORE the SD write -- settings are already in RAM
  stateSave();                               // best-effort persist; a slow/failed write no longer loses the reply
  Serial.println("Deployment + thresholds + accent saved");
}

// thresholds-only save (EDIT THRESHOLDS view) -- leaves mission metadata untouched
static void handleThresh() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad"); return; }
  readThresh(d["thresh"]);
  g_uiDirty = true;
  server.send(200, "text/plain", "ok");
  stateSave();
  Serial.println("Thresholds saved");
}

static void handleCal() {
  g_reqCal = true;                 // main loop performs the mode switch + WiFi teardown safely
  server.send(200, "text/plain", "ok");
}

// ---------------- log download ----------------
static const char *baseName(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

static bool safeLogName(const char *n) {                 // reject traversal; accept only dive*.csv
  if (!n || !*n) return false;
  if (strstr(n, "..") || strchr(n, '/') || strchr(n, '\\')) return false;
  size_t L = strlen(n);
  return L > 8 && strncmp(n, "dive", 4) == 0 && strcmp(n + L - 4, ".csv") == 0;
}

static void handleLogs() {                               // JSON list of dive*.csv with sizes
  if (!sdEnsure()) { server.send(503, "application/json", "[]"); return; }
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  File root = SD.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      const char *b = baseName(f.name());
      if (strncmp(b, "dive", 4) == 0) { JsonObject o = a.add<JsonObject>(); o["n"] = b; o["s"] = (uint32_t)f.size(); }
    }
    f.close();
  }
  root.close();
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handleLog() {                                // stream one file as an attachment
  String f = server.arg("f");
  if (!safeLogName(f.c_str())) { server.send(400, "text/plain", "bad name"); return; }
  if (!sdEnsure()) { server.send(503, "text/plain", "no sd"); return; }
  File file = SD.open(String("/") + f, FILE_READ);
  if (!file) { server.send(404, "text/plain", "not found"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=" + f);
  server.streamFile(file, "text/csv");
  file.close();
}

static void handleLogAll() {                             // every dive concatenated into one stream
  if (!sdEnsure()) { server.send(503, "text/plain", "no sd"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=dives_all.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  File root = SD.open("/");
  uint8_t buf[512];
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      const char *b = baseName(f.name());
      if (strncmp(b, "dive", 4) == 0) {
        char hdr[48]; snprintf(hdr, sizeof(hdr), "\n# ==== %s ====\n", b);
        server.sendContent(hdr);
        int n; while ((n = f.read(buf, sizeof(buf))) > 0) server.sendContent((const char *)buf, n);
      }
    }
    f.close();
  }
  root.close();
  server.sendContent("");   // terminate the chunked response
}

// delete every dive*.csv (state.json / cal.json / callog.csv are left intact).
// re-scans root each pass so we never remove a file mid-iteration; refuses while a dive is logging.
static void handleLogClear() {
  if (g_logging) { server.send(409, "text/plain", "logging"); return; }
  if (!sdEnsure()) { server.send(503, "text/plain", "no sd"); return; }
  int removed = 0;
  for (;;) {
    String target;
    File root = SD.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
      bool hit = !f.isDirectory() && strncmp(baseName(f.name()), "dive", 4) == 0;
      if (hit) target = String("/") + baseName(f.name());
      f.close();
      if (hit) break;
    }
    root.close();
    if (target.length() == 0) break;
    if (!SD.remove(target)) break;     // stop on failure rather than spin forever
    removed++;
  }
  char msg[24]; snprintf(msg, sizeof(msg), "cleared %d", removed);
  server.send(200, "text/plain", msg);
  Serial.printf("Logs cleared: %d files\n", removed);
}

void portalBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // config BEFORE softAP
  bool ok = WiFi.softAP("WaterQuality-Logger");                 // open network
  WiFi.setTxPower(WIFI_POWER_8_5dBm);                           // lower TX current -> avoid WiFi+SD brownout resets on save
  delay(100);
  Serial.printf("softAP %s, SSID=WaterQuality-Logger, IP=%s\n",
                ok ? "OK" : "FAILED", WiFi.softAPIP().toString().c_str());
  dns.start(53, "*", apIP);
  server.on("/", handleRoot);
  server.on("/api/sync",   HTTP_POST, handleSync);
  server.on("/api/state",  HTTP_GET,  handleState);
  server.on("/api/deploy", HTTP_POST, handleDeploy);
  server.on("/api/thresh", HTTP_POST, handleThresh);
  server.on("/api/cal",    HTTP_POST, handleCal);
  server.on("/api/logs",   HTTP_GET,  handleLogs);
  server.on("/api/log",    HTTP_GET,  handleLog);
  server.on("/api/logall", HTTP_GET,  handleLogAll);
  server.on("/api/logclear", HTTP_POST, handleLogClear);
  server.on("/generate_204", handle204);                 // Android / ChromeOS
  server.on("/gen_204", handle204);
  server.on("/hotspot-detect.html", handleApple);        // iOS / macOS
  server.on("/library/test/success.html", handleApple);
  server.on("/connecttest.txt", handleMsTxt);            // Windows
  server.on("/ncsi.txt", handleMsTxt);
  server.onNotFound(handleRoot);                          // any other path -> serve the config page
  server.begin();
  s_active = true;
  Serial.println("Portal up: join WiFi 'WaterQuality-Logger', then open http://192.168.4.1 in a browser");
}

void portalLoop() { if (!s_active) return; dns.processNextRequest(); server.handleClient(); }

void portalEnd() {
  if (!s_active) return;
  server.stop(); dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
  s_active = false;
  Serial.println("Portal down (submerged / WiFi off)");
}
