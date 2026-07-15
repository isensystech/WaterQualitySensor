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
static char     s_resp[600];
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

static bool nextUnsynced(char *out, size_t n) {
  File root = SD.open("/");
  if (!root) return false;
  bool found = false;
  for (File f = root.openNextFile(); f && !found; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      const char *b = strrchr(f.name(), '/'); b = b ? b + 1 : f.name();
      size_t L = strlen(b);
      if (L > 8 && !strncmp(b, "dive", 4) && !strcmp(b + L - 4, ".csv") && f.size() > 0
          && !manifestHas(b)) {
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

static void dsFail(const char *why) {           // recoverable: back off and retry later
  Serial.printf("DiveSync: FAIL (%s)\n", why);
  radioOff();
  s_backoff = s_backoff ? min(s_backoff * 2, DSYNC_BACKOFF_MAX_MS) : DSYNC_FAIL_BACKOFF_MS;
  s_next = millis() + s_backoff;
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

// ---------------- per-file upload: open file + TLS connect + request head (bounded blocking) ----------------
static bool startFile() {
  char path[24]; snprintf(path, sizeof(path), "/%s", s_fname);
  s_file = SD.open(path, FILE_READ);
  if (!s_file) { dsFail("open file"); return false; }
  parseHeader(s_file);
  snprintf(s_obj, sizeof(s_obj), "c%04lu_%s", (unsigned long)s_m.cast, s_fname);

  char host[64]; urlHost(host, sizeof(host));
  char mac[13]; macHex(mac);
  s_tls.stop();
  s_tls.setInsecure();                           // MVP (documented); hardening = pin the Supabase root CA
  if (!s_tls.connect(host, 443, 10000)) { dsFail("tls connect"); return false; }

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
  // more files? stay on this Wi-Fi and start the next one; else wrap up
  if (dsAllowed() && nextUnsynced(s_fname, sizeof(s_fname))) { startFile(); return; }
  Serial.printf("DiveSync: pass complete, %u file(s) handled\n", s_done);
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
      WiFi.scanDelete();
      if (!found) { dsQuiet(60000UL); return; }                // out of range: quiet retry, no backoff
      WiFi.begin(dsync.ssid, dsync.pass);
      WiFi.setTxPower(WIFI_POWER_8_5dBm);                      // same brownout guard as the AP side
      s_deadline = millis() + DSYNC_CONNECT_MS;
      s_ph = DS_JOIN;
      break;
    }

    case DS_JOIN:
      if (WiFi.status() == WL_CONNECTED) { startFile(); return; }
      if (WiFi.status() == WL_CONNECT_FAILED || (int32_t)(millis() - s_deadline) > 0)
        dsFail("wifi join");
      return;

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
      if (code == 200 || code == 201 || strstr(s_resp, "Duplicate") || code == 409) {
        metaPost();                                            // dup = file already up; still post the row
      } else {
        dsFail(code ? "storage http error" : "storage no response");
      }
      return;
    }

    case DS_META_RESP: {
      while (s_tls.available() && s_respLen < sizeof(s_resp) - 1) s_resp[s_respLen++] = s_tls.read();
      bool over = !s_tls.connected() && !s_tls.available();
      if (!over && (int32_t)(millis() - s_deadline) < 0) return;
      int code = respCode();
      if (code == 201 || (code == 409 && strstr(s_resp, "23505"))) fileDone("OK");
      else if (code == 409 && strstr(s_resp, "23503"))            fileDone("REJECTED");  // not allowlisted
      else dsFail(code ? "meta http error" : "meta no response");
      return;
    }
  }
}
