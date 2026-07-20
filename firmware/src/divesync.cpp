// DiveSync — autonomous post-dive offload to Supabase (docs/DiveSync-To-Do.md Phases 1/2/4).
//
// HARD CONSTRAINT (do not weaken): this machine advances ONLY when surfaced and idle —
// !g_logging && !g_submerged, portal AP down, run mode. The dive loop is untouched: no Wi-Fi
// polling, no scanning, no radio while logging. If the unit submerges mid-sync, everything is
// aborted on the spot (Wi-Fi off before the logging gate can even open — it debounces 3 samples).
//
// Cloud contract (REVISED + live-verified 2026-07-14/15, see DiveSync-To-Do.md Phase 4):
//   1. POST /storage/v1/object/dives/<mac>/<obj>  — raw CSV, streamed off SD in DSYNC_CHUNK
//      pieces, one chunk per loop() pass so sampling/UI stay alive. NO x-upsert.
//   2. POST /rest/v1/dives — metadata JSON parsed from the file's own '#' header (not the live
//      deploy struct: old files must carry THEIR mission, not the current one).
//   200/201 = done; 409/"Duplicate"/23505 = already synced (mark done); 23503 = MAC not on the
//   allowlist (mark REJECTED — do not retry forever). Anything else = backoff and retry later.
//
// The upload object name is "c<cast>_<file>" (cast parsed from the header): dive filenames
// restart at dive0000 after a "Clear logs", but the cast counter only ever climbs, so a cleared
// card can never silently collide with (and be shadowed by) an older upload of the same name.
//
// Blocking exceptions, deliberate + bounded (same spirit as the OTA rule-5 exception): the TLS
// handshake (~1-3 s) and Wi-Fi join are surface-only, never-while-logging, and deadline-capped.
// Everything else — scan, pump, response reads — is non-blocking millis() state machine.

#include "shared.h"
#include <WiFiClientSecure.h>
#include <esp_mac.h>

DiveSyncCfg dsync = {};

void diveSyncDefaults() {
  dsync.mode = SYNC_NONE;                       // opt-in: upgraded units behave exactly as before
  dsync.ssid[0] = dsync.pass[0] = 0;
  strncpy(dsync.url, DSYNC_URL_DEFAULT, sizeof(dsync.url) - 1);
  strncpy(dsync.key, DSYNC_KEY_DEFAULT, sizeof(dsync.key) - 1);
}

// ---------------- runtime state ----------------
enum : uint8_t { DS_IDLE, DS_SCAN, DS_JOIN, DS_PUMP, DS_RESP, DS_META_RESP };
static uint8_t  s_ph       = DS_IDLE;
static uint32_t s_next     = 0;     // DS_IDLE: don't re-check gates before this millis()
static uint32_t s_deadline = 0;     // per-phase timeout
static uint32_t s_backoff  = 0;     // current failure backoff (0 = none yet)
static WiFiClientSecure s_tls;
static File     s_file;
static char     s_fname[20];        // "dive0007.csv" (basename on SD)
static char     s_obj[28];          // "c0012_dive0007.csv" (cloud object / filename key)
static uint32_t s_nl, s_hdrNl;      // newline counts: whole file / header ('#' lines)
// MUST outrun the response HEADERS or the body is never seen. Supabase behind Cloudflare sends
// ~790 bytes of headers (a ~300-byte set-cookie alone) before the JSON body — at the old 600 the
// buffer filled with headers, so the "Duplicate" / "23505" markers in the body could never match
// and an already-uploaded file wedged its retry loop forever (v0.10.9 field bug).
static char     s_resp[1600];
static size_t   s_respLen;
static uint8_t  s_done;             // files synced this pass (serial summary)

// parsed '#' meta header of the file being uploaded
static struct {
  char utc[24], tsrc[16], mission[32], op[24], site[48], wt[12], wx[24], notes[64], cycu[8];
  double lat, lon; float airC;
  bool hasPos, hasWx, calPh, calEc, calOrp, calCyc, poet, bar30, cels, cyc;
  uint32_t cast;
} s_m;

bool diveSyncBusy() { return s_ph != DS_IDLE; }

// One-line human status, shown live in the portal's Data offload card (/api/state ds_stat)
// and updated at every phase transition — so a field user can see WHY a sync isn't happening
// without a serial cable (v0.10.4: the "SSID not seen" path used to be completely silent).
static char s_status[64] = "idle";
const char *diveSyncStatusText() { return s_status; }
static void setStatus(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(s_status, sizeof(s_status), fmt, ap);
  va_end(ap);
}

// ---------------- small helpers ----------------
static void macHex(char out[13]) {              // base (STA) MAC, lowercase hex, no colons
  uint8_t m[6]; esp_read_mac(m, ESP_MAC_WIFI_STA);
  snprintf(out, 13, "%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static const char *urlHost(char *buf, size_t n) {   // "https://x.y.z" -> "x.y.z"
  const char *h = dsync.url;
  if (!strncmp(h, "https://", 8)) h += 8;
  size_t i = 0;
  while (h[i] && h[i] != '/' && i < n - 1) { buf[i] = h[i]; i++; }
  buf[i] = 0;
  return buf;
}

// JSON string escape: quotes/backslash escaped, control chars flattened to spaces.
static void jesc(char *dst, size_t n, const char *src) {
  size_t o = 0;
  for (const char *p = src; *p && o < n - 2; p++) {
    if (*p == '"' || *p == '\\') { if (o >= n - 3) break; dst[o++] = '\\'; dst[o++] = *p; }
    else dst[o++] = ((uint8_t)*p < 0x20) ? ' ' : *p;
  }
  dst[o] = 0;
}

// ---------------- sync manifest (append-only /sync.csv: filename,epoch,status) ----------------
static bool manifestHas(const char *fname) {
  File f = SD.open(DSYNC_MANIFEST, FILE_READ);
  if (!f) return false;
  bool hit = false;
  while (f.available() && !hit) {
    String ln = f.readStringUntil('\n');
    int c = ln.indexOf(',');
    if (c > 0 && ln.substring(0, c) == fname) hit = true;
  }
  f.close();
  return hit;
}

static void manifestMark(const char *fname, const char *status) {
  File f = SD.open(DSYNC_MANIFEST, FILE_APPEND);
  if (!f) return;
  f.printf("%s,%lu,%s\n", fname, (unsigned long)nowUnix(), status);
  f.close();
}

// ---------------- persistent attempt trace (append-only /synclog.csv: epoch,ms,event) ----------------
// The ONLY sync record a sealed unit can surface after the fact: no serial cable, no way to pull the
// SD, and the portal (which serves the live ds_stat) is torn down for the whole attempt. Read it back
// over the portal via GET /api/diag once the AP returns. Called only at quiescent points (radio idle
// or just powered off) so the append never coincides with Wi-Fi TX — the documented brownout hazard.
static void syncLog(const char *fmt, ...) {
  if (!g_sdReady) return;
  if (SD.exists(DSYNC_SYNCLOG)) {                 // simple rotate: wipe once it grows past the cap
    File chk = SD.open(DSYNC_SYNCLOG, FILE_READ);
    bool over = chk && chk.size() > DSYNC_SYNCLOG_CAP;
    if (chk) chk.close();
    if (over) SD.remove(DSYNC_SYNCLOG);
  }
  File f = SD.open(DSYNC_SYNCLOG, FILE_APPEND);
  if (!f) return;
  char msg[96];
  va_list ap; va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  f.printf("%lu,%lu,%s\n", (unsigned long)nowUnix(), (unsigned long)millis(), msg);
  f.close();
}

// Head-of-line guard (v0.10.9). nextUnsynced() hands back the OLDEST unsynced file, so a single
// file that fails every pass stalls every newer dive behind it indefinitely — dive0003 did exactly
// that. After DSYNC_MAX_FILE_FAILS consecutive failures the file is deferred for this power cycle
// so the queue drains; a reboot clears the list and it is retried from scratch.
static char    s_defer[4][20];
static uint8_t s_deferN = 0;
static char    s_failFile[20] = "";
static uint8_t s_failCnt = 0;
static bool isDeferred(const char *f) {
  for (uint8_t i = 0; i < s_deferN; i++) if (!strcmp(s_defer[i], f)) return true;
  return false;
}
static void noteFailure() {
  if (!s_fname[0]) return;
  if (strcmp(s_failFile, s_fname)) { strncpy(s_failFile, s_fname, sizeof(s_failFile) - 1);
                                     s_failFile[sizeof(s_failFile) - 1] = 0; s_failCnt = 1; }
  else s_failCnt++;
  if (s_failCnt >= DSYNC_MAX_FILE_FAILS && s_deferN < 4 && !isDeferred(s_fname)) {
    strncpy(s_defer[s_deferN], s_fname, 19); s_defer[s_deferN][19] = 0; s_deferN++;
    syncLog("deferring %s after %u fails - moving on to newer dives", s_fname, (unsigned)s_failCnt);
    s_failCnt = 0;
  }
}

static bool nextUnsynced(char *out, size_t n) {
  File root = SD.open("/");
  if (!root) return false;
  bool found = false;
  for (File f = root.openNextFile(); f && !found; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      const char *b = strrchr(f.name(), '/'); b = b ? b + 1 : f.name();
      size_t L = strlen(b);
      if (L > 8 && !strncmp(b, "dive", 4) && !strcmp(b + L - 4, ".csv") && f.size() > 0
          && !manifestHas(b) && !isDeferred(b)) {
        strncpy(out, b, n - 1); out[n - 1] = 0; found = true;
      }
    }
    f.close();
  }
  root.close();
  return found;
}

// ---------------- '#' header parse (the file's OWN metadata, not the live deploy struct) ----------------
static void metaLine(const char *ln) {
  const char *v;
  if      ((v = strstr(ln, "# utc_start: ")))   { strncpy(s_m.utc,  v + 13, sizeof(s_m.utc) - 1); }
  else if ((v = strstr(ln, "# time_source: "))) { strncpy(s_m.tsrc, v + 15, sizeof(s_m.tsrc) - 1); }
  else if ((v = strstr(ln, "# cast: ")))        { s_m.cast = strtoul(v + 8, nullptr, 10); }
  else if ((v = strstr(ln, "# mission: ")))     { strncpy(s_m.mission, v + 11, sizeof(s_m.mission) - 1); }
  else if ((v = strstr(ln, "# operator: ")))    { strncpy(s_m.op,   v + 12, sizeof(s_m.op) - 1); }
  else if ((v = strstr(ln, "# site: ")))        { strncpy(s_m.site, v + 8,  sizeof(s_m.site) - 1); }
  else if ((v = strstr(ln, "# water_type: ")))  { strncpy(s_m.wt,   v + 14, sizeof(s_m.wt) - 1); }
  else if ((v = strstr(ln, "# gps: ")))         { s_m.lat = atof(v + 7);
                                                  const char *c = strchr(v + 7, ',');
                                                  if (c) { s_m.lon = atof(c + 1); s_m.hasPos = true; } }
  else if ((v = strstr(ln, "# weather: ")))     { const char *a = strstr(v, "  air_C: ");
                                                  size_t L = a ? (size_t)(a - (v + 11)) : strlen(v + 11);
                                                  if (L >= sizeof(s_m.wx)) L = sizeof(s_m.wx) - 1;
                                                  memcpy(s_m.wx, v + 11, L); s_m.wx[L] = 0;
                                                  if (a) { s_m.airC = atof(a + 9); s_m.hasWx = true; } }
  else if ((v = strstr(ln, "# notes: ")))       { strncpy(s_m.notes, v + 9, sizeof(s_m.notes) - 1); }
  else if (strstr(ln, "# cal_ph: "))            { s_m.calPh  = strstr(ln, "cal_ph: Y");
                                                  s_m.calEc  = strstr(ln, "cal_ec: Y");
                                                  s_m.calOrp = strstr(ln, "cal_orp: Y");
                                                  s_m.calCyc = strstr(ln, "cal_cyc: Y"); }
  else if (strstr(ln, "# sensors: "))           { s_m.poet  = strstr(ln, "POET=on");
                                                  s_m.bar30 = strstr(ln, "BAR30=on");
                                                  s_m.cels  = strstr(ln, "CELS=on");
                                                  s_m.cyc   = strstr(ln, "CYC=on"); }
  else if ((v = strstr(ln, "# cyclops_units: "))) { strncpy(s_m.cycu, v + 17, sizeof(s_m.cycu) - 1); }
}

static void parseHeader(File &f) {              // reads the leading '#' block; leaves f positioned after it
  memset(&s_m, 0, sizeof(s_m));
  s_hdrNl = 0;
  while (f.available()) {
    uint32_t pos = f.position();
    String ln = f.readStringUntil('\n');
    if (!ln.startsWith("#")) { f.seek(pos); break; }   // column header line: put it back
    ln.trim();
    metaLine(ln.c_str());
    s_hdrNl++;
    if (s_hdrNl > 32) break;                           // defensive cap; real headers are ~13 lines
  }
}

// metadata JSON per the revised Phase 4 contract; strings escaped, unsynced clock -> null utc_start
static size_t buildMetaJson(char *b, size_t n, const char *mac, uint32_t rows) {
  char e[128]; size_t o = 0;
  o += snprintf(b + o, n - o, "{\"device_id\":\"%s\",\"filename\":\"%s\",\"storage_path\":\"%s/%s\"",
                mac, s_obj, mac, s_obj);
  o += snprintf(b + o, n - o, ",\"cast_num\":%lu", (unsigned long)s_m.cast);
  jesc(e, sizeof(e), s_m.mission); o += snprintf(b + o, n - o, ",\"mission\":\"%s\"", e);
  jesc(e, sizeof(e), s_m.op);      o += snprintf(b + o, n - o, ",\"operator\":\"%s\"", e);
  jesc(e, sizeof(e), s_m.site);    o += snprintf(b + o, n - o, ",\"site\":\"%s\"", e);
  jesc(e, sizeof(e), s_m.wt);      o += snprintf(b + o, n - o, ",\"water_type\":\"%s\"", e);
  if (s_m.hasPos) o += snprintf(b + o, n - o, ",\"lat\":%.6f,\"lon\":%.6f", s_m.lat, s_m.lon);
  if (s_m.hasWx) { jesc(e, sizeof(e), s_m.wx);
                   o += snprintf(b + o, n - o, ",\"weather\":\"%s\",\"air_temp_c\":%.1f", e, s_m.airC); }
  jesc(e, sizeof(e), s_m.notes);   o += snprintf(b + o, n - o, ",\"notes\":\"%s\"", e);
  if (strcmp(s_m.utc, "unsynced") && s_m.utc[0])
       o += snprintf(b + o, n - o, ",\"utc_start\":\"%s\"", s_m.utc);
  else o += snprintf(b + o, n - o, ",\"utc_start\":null");
  o += snprintf(b + o, n - o, ",\"time_source\":\"%s\"", s_m.tsrc[0] ? s_m.tsrc : "UNSYNCED");
  o += snprintf(b + o, n - o, ",\"cal_ph\":%s,\"cal_ec\":%s,\"cal_orp\":%s,\"cal_cyc\":%s",
                s_m.calPh ? "true" : "false", s_m.calEc ? "true" : "false",
                s_m.calOrp ? "true" : "false", s_m.calCyc ? "true" : "false");
  o += snprintf(b + o, n - o, ",\"poet_en\":%s,\"bar30_en\":%s,\"cels_en\":%s,\"cyc_en\":%s",
                s_m.poet ? "true" : "false", s_m.bar30 ? "true" : "false",
                s_m.cels ? "true" : "false", s_m.cyc ? "true" : "false");
  if (s_m.cycu[0]) { jesc(e, sizeof(e), s_m.cycu);
                     o += snprintf(b + o, n - o, ",\"cyclops_units\":\"%s\"", e); }
  else               o += snprintf(b + o, n - o, ",\"cyclops_units\":null");
  o += snprintf(b + o, n - o, ",\"row_count\":%lu}", (unsigned long)rows);
  return o;
}

// ---------------- teardown paths ----------------
static void radioOff() {
  s_tls.stop();
  if (s_file) s_file.close();
  WiFi.scanDelete();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

static void dsFail(const char *fmt, ...) {      // recoverable: back off and retry later (printf-style)
  char why[80];
  va_list ap; va_start(ap, fmt); vsnprintf(why, sizeof(why), fmt, ap); va_end(ap);
  Serial.printf("DiveSync: FAIL (%s)\n", why);
  radioOff();
  s_backoff = s_backoff ? min(s_backoff * 2, DSYNC_BACKOFF_MAX_MS) : DSYNC_FAIL_BACKOFF_MS;
  s_next = millis() + s_backoff;
  setStatus("failed: %s (retry in %lu min)", why, (unsigned long)(s_backoff / 60000UL));
  syncLog("FAILED: %s (%s, retry in %lu min)", why, s_fname, (unsigned long)(s_backoff / 60000UL));
  noteFailure();                                 // defer a file that keeps failing (head-of-line guard)
  s_ph = DS_IDLE;
}

static void dsQuiet(uint32_t delayMs) {         // no-fault retreat (AP not in range, submerged...)
  radioOff();
  s_next = millis() + delayMs;
  s_ph = DS_IDLE;
}

void diveSyncCancel(const char *why) {
  if (s_ph == DS_IDLE) return;
  Serial.printf("DiveSync: cancelled (%s)\n", why);
  setStatus("cancelled (%s)", why);
  dsQuiet(DSYNC_CHECK_MS);
}

void diveSyncKick() {                           // a dive just closed: try promptly, forgive old failures
  s_backoff = 0;
  s_next = millis() + DSYNC_CHECK_MS / 2;
}

// The one gate that guards EVERY phase. Any dive activity kills the sync instantly.
static bool dsAllowed() {
  return g_mode == MODE_RUN && !g_recovery && !g_logging && !g_submerged &&
         !portalActive() && g_sdReady && dsync.mode == SYNC_CLOUD && dsync.ssid[0];
}

// Diagnostic probe for /api/wifitest (v0.10.8). Assumes the STA is ALREADY joined; walks the rest
// of the upload path — DNS, then TLS to the configured cloud host — and reports which stage broke.
// Lets the whole chain be proven from the portal instead of burning a dive cycle per attempt.
void diveSyncProbe(char *out, size_t n) {
  char host[64]; urlHost(host, sizeof(host));
  IPAddress hip;
  if (!WiFi.hostByName(host, hip)) { snprintf(out, n, "DNS FAILED for %s", host); return; }
  WiFiClientSecure tls;
  tls.setInsecure();
  uint32_t t0 = millis();
  if (!tls.connect(host, 443, DSYNC_TLS_MS)) {
    snprintf(out, n, "TLS FAILED (dns ok %s, rssi %d, heap %u)", hip.toString().c_str(),
             (int)WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
    tls.stop(); return;
  }
  snprintf(out, n, "DNS+TLS OK (%s, %lu ms, rssi %d)", hip.toString().c_str(),
           (unsigned long)(millis() - t0), (int)WiFi.RSSI());
  tls.stop();
}

// ---------------- per-file upload: open file + TLS connect + request head (bounded blocking) ----------------
static bool startFile() {
  char path[24]; snprintf(path, sizeof(path), "/%s", s_fname);
  s_file = SD.open(path, FILE_READ);
  if (!s_file) { dsFail("open file"); return false; }
  parseHeader(s_file);
  snprintf(s_obj, sizeof(s_obj), "c%04lu_%s", (unsigned long)s_m.cast, s_fname);

  char host[64]; urlHost(host, sizeof(host));
  char mac[13]; macHex(mac);
  WiFi.setTxPower(DSYNC_TX_PUMP);
  s_tls.stop();
  s_tls.setInsecure();                           // MVP (documented); hardening = pin the Supabase root CA
  // Split DNS from the handshake: "tls connect" alone couldn't tell a name-resolution failure from
  // a dead link, and both look identical on the run screen. Carry RSSI/heap for the marginal cases.
  IPAddress hip;
  if (!WiFi.hostByName(host, hip)) { dsFail("dns failed for %s (rssi %d)", host, (int)WiFi.RSSI()); return false; }
  if (!s_tls.connect(host, 443, DSYNC_TLS_MS)) {
    dsFail("tls connect failed (dns ok %s, rssi %d, heap %u)", hip.toString().c_str(),
           (int)WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
    return false;
  }

  // The WHOLE file streams to the cloud including the '#' block — the cloud CSV must be
  // byte-identical to the SD file — so rewind after the header parse before sizing/sending.
  s_file.seek(0);
  char head[320];
  int hl = snprintf(head, sizeof(head),
                    "POST /storage/v1/object/dives/%s/%s HTTP/1.1\r\n"
                    "Host: %s\r\napikey: %s\r\nContent-Type: text/csv\r\n"
                    "Content-Length: %lu\r\nConnection: close\r\n\r\n",
                    mac, s_obj, host, dsync.key, (unsigned long)s_file.size());
  if (s_tls.write((const uint8_t *)head, hl) != (size_t)hl) { dsFail("tls write head"); return false; }
  s_nl = 0;                                      // recount from byte 0 while pumping
  s_respLen = 0;
  s_ph = DS_PUMP;
  Serial.printf("DiveSync: uploading %s (%lu bytes) as %s\n",
                s_fname, (unsigned long)s_file.size(), s_obj);
  setStatus("uploading %s", s_fname);
  return true;
}

static void metaPost() {                         // storage leg done -> post the metadata row
  char host[64]; urlHost(host, sizeof(host));
  char mac[13]; macHex(mac);
  uint32_t rows = (s_nl > s_hdrNl + 1) ? (s_nl - s_hdrNl - 1) : 0;   // minus '#' block + column line
  static char body[1024];
  size_t bl = buildMetaJson(body, sizeof(body), mac, rows);
  s_tls.stop();
  s_tls.setInsecure();
  if (!s_tls.connect(host, 443, 10000)) { dsFail("tls connect meta"); return; }
  char head[256];
  int hl = snprintf(head, sizeof(head),
                    "POST /rest/v1/dives HTTP/1.1\r\nHost: %s\r\napikey: %s\r\n"
                    "Content-Type: application/json\r\nPrefer: return=minimal\r\n"
                    "Content-Length: %u\r\nConnection: close\r\n\r\n",
                    host, dsync.key, (unsigned)bl);
  if (s_tls.write((const uint8_t *)head, hl) != (size_t)hl ||
      s_tls.write((const uint8_t *)body, bl) != bl) { dsFail("tls write meta"); return; }
  s_respLen = 0;
  s_deadline = millis() + DSYNC_HTTP_MS;
  s_ph = DS_META_RESP;
}

// First ~90 chars of the response BODY, flattened for the one-line CSV trace (commas/newlines out).
static void respBody(char *out, size_t n) {
  const char *b = strstr(s_resp, "\r\n\r\n");
  b = b ? b + 4 : s_resp;
  size_t i = 0;
  while (*b && i < n - 1) { char c = *b++; if (c == '\r' || c == '\n' || c == ',') c = ' '; out[i++] = c; }
  out[i] = 0;
}
// "already uploaded on an earlier pass". Storage answers a duplicate with HTTP *400* and the real
// 409 only inside the body ({"statusCode":"409","error":"Duplicate"}), so the status line alone
// can't be trusted — match the body marker too.
static bool respIsDuplicate() {
  return strstr(s_resp, "Duplicate") || strstr(s_resp, "already exists");
}

static int respCode() {                          // "HTTP/1.1 201 ..." -> 201 (0 = unparsable)
  s_resp[s_respLen] = 0;
  if (s_respLen < 12 || strncmp(s_resp, "HTTP/", 5)) return 0;
  const char *sp = strchr(s_resp, ' ');
  return sp ? atoi(sp + 1) : 0;
}

static void fileDone(const char *status) {
  manifestMark(s_fname, status);
  s_tls.stop();
  s_done++;
  Serial.printf("DiveSync: %s -> %s\n", s_fname, status);
  syncLog("%s -> %s", s_fname, status);
  if (!strcmp(status, "REJECTED")) setStatus("%s REJECTED - MAC not on allowlist", s_fname);
  // more files? stay on this Wi-Fi and start the next one; else wrap up
  if (dsAllowed() && nextUnsynced(s_fname, sizeof(s_fname))) { startFile(); return; }
  Serial.printf("DiveSync: pass complete, %u file(s) handled\n", s_done);
  syncLog("pass complete, %u file(s) handled", s_done);
  if (strcmp(status, "REJECTED")) setStatus("synced %u file(s)", s_done);
  s_backoff = 0;
  dsQuiet(DSYNC_CHECK_MS);
}

// ---------------- the state machine (one small step per loop pass) ----------------
void diveSyncLoop() {
  if (s_ph != DS_IDLE && !dsAllowed()) { diveSyncCancel("dive/portal activity"); return; }

  switch (s_ph) {
    case DS_IDLE: {
      if ((int32_t)(millis() - s_next) < 0) return;
      s_next = millis() + DSYNC_CHECK_MS;
      if (!dsAllowed()) return;
      if (!nextUnsynced(s_fname, sizeof(s_fname))) return;    // nothing new to send
      WiFi.persistent(false);
      WiFi.mode(WIFI_STA);
      WiFi.scanNetworks(true);                                 // async; poll below
      s_deadline = millis() + DSYNC_SCAN_MS;
      s_done = 0;
      s_ph = DS_SCAN;
      Serial.printf("DiveSync: %s pending, scanning for '%s'\n", s_fname, dsync.ssid);
      setStatus("scanning for '%s'", dsync.ssid);
      syncLog("attempt: %s pending, scanning for '%s'", s_fname, dsync.ssid);
      break;
    }

    case DS_SCAN: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        if ((int32_t)(millis() - s_deadline) > 0) dsFail("scan stuck");
        return;
      }
      bool found = false;
      for (int i = 0; i < n && !found; i++) found = (WiFi.SSID(i) == dsync.ssid);
      if (!found) {
        // This path was SILENT before v0.10.4 and it is the most common field failure
        // (5 GHz-only hotspot, dormant hotspot, SSID typo) — say exactly what we saw.
        Serial.printf("DiveSync: '%s' NOT seen in scan (%d networks):\n", dsync.ssid, n);
        for (int i = 0; i < n && i < 10; i++)
          Serial.printf("  %s (%d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        setStatus("'%s' not seen (%d nearby)", dsync.ssid, n);
        WiFi.scanDelete();
        dsQuiet(60000UL);                                      // out of range: quiet retry, no backoff
        syncLog("'%s' NOT seen in scan (%d networks nearby)", dsync.ssid, n);
        return;
      }
      WiFi.scanDelete();
      Serial.printf("DiveSync: '%s' found, joining\n", dsync.ssid);
      setStatus("joining '%s'", dsync.ssid);
      WiFi.setTxPower(DSYNC_TX_JOIN);                          // full power BEFORE begin: associate through the housing
      WiFi.begin(dsync.ssid, dsync.pass);
      s_deadline = millis() + DSYNC_CONNECT_MS;
      s_ph = DS_JOIN;
      break;
    }

    case DS_JOIN: {
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED) {
        syncLog("joined '%s' (rssi %d dBm, ip %s)", dsync.ssid, (int)WiFi.RSSI(),
                WiFi.localIP().toString().c_str());
        startFile(); return;
      }
      // Distinguish a real credential/security reject from a plain no-association timeout — the old
      // code blamed the password for both. Raw wl_status: 1=no-SSID 4=CONNECT_FAILED 6=DISCONNECTED.
      if (st == WL_CONNECT_FAILED)              dsFail("auth rejected by '%s' (check password/security)", dsync.ssid);
      else if ((int32_t)(millis() - s_deadline) > 0) dsFail("join timeout, no association (wl_status %d)", (int)st);
      return;
    }

    case DS_PUMP: {
      if (!s_tls.connected()) { dsFail("conn dropped"); return; }
      uint8_t buf[DSYNC_CHUNK];
      int n = s_file.read(buf, sizeof(buf));
      if (n > 0) {
        if (s_tls.write(buf, n) != (size_t)n) { dsFail("tls write"); return; }
        for (int i = 0; i < n; i++) if (buf[i] == '\n') s_nl++;
        return;                                                // ONE chunk per pass — loop stays live
      }
      s_file.close();                                          // whole file sent; await the verdict
      s_respLen = 0;
      s_deadline = millis() + DSYNC_HTTP_MS;
      s_ph = DS_RESP;
      return;
    }

    case DS_RESP: {
      while (s_tls.available() && s_respLen < sizeof(s_resp) - 1) s_resp[s_respLen++] = s_tls.read();
      bool over = !s_tls.connected() && !s_tls.available();
      if (!over && (int32_t)(millis() - s_deadline) < 0) return;
      int code = respCode();
      if (code == 200 || code == 201 || code == 409 || respIsDuplicate()) {
        metaPost();                                            // dup = file already up; still post the row
      } else {
        char b[96]; respBody(b, sizeof(b));
        syncLog("storage HTTP %d: %s", code, b);
        dsFail(code ? "storage http %d" : "storage no response", code);
      }
      return;
    }

    case DS_META_RESP: {
      while (s_tls.available() && s_respLen < sizeof(s_resp) - 1) s_resp[s_respLen++] = s_tls.read();
      bool over = !s_tls.connected() && !s_tls.available();
      if (!over && (int32_t)(millis() - s_deadline) < 0) return;
      int code = respCode();
      // Match the PostgREST error codes in the BODY regardless of the status line (same
      // header-overrun lesson as storage): 23505 = row already there, 23503 = MAC not allowlisted.
      if (code == 200 || code == 201 || strstr(s_resp, "23505") || strstr(s_resp, "duplicate key")) fileDone("OK");
      else if (strstr(s_resp, "23503"))                                                             fileDone("REJECTED");
      else {
        char b[96]; respBody(b, sizeof(b));
        syncLog("meta HTTP %d: %s", code, b);
        dsFail(code ? "meta http %d" : "meta no response", code);
      }
      return;
    }
  }
}
