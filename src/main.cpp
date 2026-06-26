#include "shared.h"

// ---------------- global definitions ----------------
Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
MS5837    bar30;
Adafruit_ADS1015 ads;          // Qwiic ADS1015 for the Cyclops-7F (optional)
bool      g_cycOk = false;
float     g_cycV  = 0;
PoetResult g_poet = {0,0,0,0,0};
bool      g_poetOk = false, g_submerged = false, g_sdReady = false, g_logging = false;
float     g_P = 0, g_depth = 0, g_barT = 0, g_ascent = 0, g_lastDepth = 0;
uint32_t  g_sampleCount = 0, g_poiCount = 0, g_lastSampleMs = 0;
uint8_t   g_page = 0;
AppMode   g_mode = MODE_RUN;
volatile NavEvent g_nav = NAV_NONE;
CalData   cal = {false,0,0,0, false,0,0, false,0, 0,0,0};
Deployment deploy = {"", "", "", "", "", "", 0,0,false, 0,false, 0, false, 0};  // accent + thresh[] zero-init
uint16_t  g_accent = ST77XX_CYAN;

bool      g_timeSynced = false, g_timeApprox = false;
uint32_t  g_epoch0 = 0, g_epochAtMillis = 0;

static File     g_logFile;
static char     g_logPath[16] = "";
static int      g_subStreak = 0, g_surfStreak = 0;
static uint32_t g_poiBannerUntil = 0;
static uint8_t  g_bannerKind = 0;        // 0 = POI logged, 1 = idle acknowledgement
volatile bool   g_reqCal = false;
volatile bool   g_uiDirty = false;
bool            g_recovery = false;      // upload-only recovery AP mode (set at boot, never logs/samples)

// ---- backlight auto-dim state ----
enum { BL_FULL, BL_FADING, BL_OFF };
static uint8_t  g_blState        = BL_FULL;
static uint16_t g_blLevel        = BL_LEVEL_MAX;   // current PWM duty (0..BL_LEVEL_MAX)
static uint32_t g_blLastActivity = 0;              // millis() of last twist (idle timer base)
static uint32_t g_blFadeStart    = 0;              // millis() the fade began

uint16_t accentColor(uint8_t idx) {
  switch (idx) {
    case 1:  return 0xFD20;        // orange
    case 2:  return ST77XX_GREEN;  // green
    case 3:  return ST77XX_RED;    // red
    default: return ST77XX_CYAN;   // teal (default)
  }
}

// ---------------- backlight auto-dim ----------------
// Power saver: after dimMinutesEffective() of no twist activity, fade the backlight off.
// The screen content keeps being rendered underneath; the next twist just wakes it.
uint16_t dimMinutesEffective() {
  uint16_t m = deploy.dimMinutes;
  if (m < DIM_MIN_MIN || m > DIM_MIN_MAX) return DIM_MIN_DEFAULT;
  return m;
}

static void blApply(uint16_t lvl) {            // write PWM duty only when it actually changes
  if (lvl == g_blLevel) return;
  g_blLevel = lvl;
  ledcWrite(PIN_BL, lvl);
}

void backlightBegin() {
  ledcAttach(PIN_BL, BL_PWM_FREQ, BL_PWM_RES);
  g_blLevel = BL_LEVEL_MAX;
  ledcWrite(PIN_BL, BL_LEVEL_MAX);
  g_blState = BL_FULL;
  g_blLastActivity = millis();
}

// Restore full brightness and reset the idle timer. Returns true if the screen WAS dimmed/off,
// so the caller can swallow the waking twist (no POI / no page flip on that first press).
bool backlightWake() {
  bool wasDim = (g_blState != BL_FULL);
  blApply(BL_LEVEL_MAX);
  g_blState = BL_FULL;
  g_blLastActivity = millis();
  if (wasDim && g_mode == MODE_RUN) g_uiDirty = true;   // refresh the just-revealed frame
  return wasDim;
}

void backlightLoop() {
  // Dimming applies only in run mode; calibration is a bench task on USB power -> stay lit.
  if (g_mode != MODE_RUN) {
    if (g_blState != BL_FULL) { blApply(BL_LEVEL_MAX); g_blState = BL_FULL; g_blLastActivity = millis(); }
    return;
  }
  uint32_t now = millis();
  uint32_t timeoutMs = (uint32_t)dimMinutesEffective() * 60000UL;
  switch (g_blState) {
    case BL_FULL:
      if (now - g_blLastActivity >= timeoutMs) { g_blState = BL_FADING; g_blFadeStart = now; }
      break;
    case BL_FADING: {
      uint32_t el = now - g_blFadeStart;
      if (el >= BL_FADE_MS) { blApply(0); g_blState = BL_OFF; }
      else blApply((uint16_t)((uint32_t)BL_LEVEL_MAX * (BL_FADE_MS - el) / BL_FADE_MS));
      break;
    }
    case BL_OFF:
    default:
      break;
  }
}

// ---------------- time helpers ----------------
uint32_t nowUnix() {
  if (!g_timeSynced) return 0;
  return g_epoch0 + (millis() - g_epochAtMillis) / 1000UL;
}
void isoTime(uint32_t t, char *out, size_t n) {
  if (!t) { snprintf(out, n, "unsynced"); return; }
  time_t tt = (time_t)t; struct tm g; gmtime_r(&tt, &g);
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
}

// ---------------- POET (non-blocking) ----------------
bool poetStart() {
  Wire.beginTransmission(POET_ADDR); Wire.write(0x0F);
  return Wire.endTransmission() == 0;
}
bool poetFetch(PoetResult &o) {
  const uint8_t n = 20;
  if (Wire.requestFrom(POET_ADDR, n) != n) { while (Wire.available()) Wire.read(); return false; }
  uint8_t b[n]; for (uint8_t i = 0; i < n; i++) b[i] = Wire.read();
  o.temp_mC = (int32_t)((uint32_t)b[0]  | ((uint32_t)b[1]<<8)  | ((uint32_t)b[2]<<16)  | ((uint32_t)b[3]<<24));
  o.orp_uV  = (int32_t)((uint32_t)b[4]  | ((uint32_t)b[5]<<8)  | ((uint32_t)b[6]<<16)  | ((uint32_t)b[7]<<24));
  o.ugs_uV  = (int32_t)((uint32_t)b[8]  | ((uint32_t)b[9]<<8)  | ((uint32_t)b[10]<<16) | ((uint32_t)b[11]<<24));
  o.ec_nA   = (int32_t)((uint32_t)b[12] | ((uint32_t)b[13]<<8) | ((uint32_t)b[14]<<16) | ((uint32_t)b[15]<<24));
  o.ec_uV   = (int32_t)((uint32_t)b[16] | ((uint32_t)b[17]<<8) | ((uint32_t)b[18]<<16) | ((uint32_t)b[19]<<24));
  return true;
}

// ---------------- SD ----------------
static const char *sdTypeName() {
  switch (SD.cardType()) { case CARD_NONE: return "NONE"; case CARD_MMC: return "MMC";
    case CARD_SD: return "SD"; case CARD_SDHC: return "SDHC"; default: return "UNKNOWN"; }
}
static bool sdMount() {
  for (int a = 1; a <= 3; a++) {
    if (SD.begin(PIN_SD_CS, SPI, SD_SPI_HZ)) { if (SD.cardType() != CARD_NONE) return true; SD.end(); }
    delay(250);
  }
  return false;
}
bool sdEnsure() {                 // mount if not already; used before any SD write
  if (g_sdReady) return true;
  g_sdReady = sdMount();
  return g_sdReady;
}
static void nextLogPath(char *out, size_t n) {
  for (int i = 0; i < 10000; i++) { snprintf(out, n, "/dive%04d.csv", i); if (!SD.exists(out)) return; }
  snprintf(out, n, "/dive_ovf.csv");
}

static void writeMetaHeader() {
  char ts[24]; isoTime(nowUnix(), ts, sizeof(ts));
  g_logFile.printf("# file: %s\n", g_logPath);
  g_logFile.printf("# utc_start: %s\n", ts);
  g_logFile.printf("# time_source: %s\n", g_timeSynced ? (g_timeApprox ? "PHONE_APPROX" : "PHONE") : "UNSYNCED");
  g_logFile.printf("# cast: %lu\n", (unsigned long)deploy.castNum);
  if (deploy.set) {
    g_logFile.printf("# mission: %s\n", deploy.mission);
    g_logFile.printf("# operator: %s\n", deploy.op);
    g_logFile.printf("# site: %s\n", deploy.site);
    g_logFile.printf("# water_type: %s\n", deploy.waterType);
    if (deploy.hasPos)     g_logFile.printf("# gps: %.6f,%.6f\n", deploy.lat, deploy.lon);
    if (deploy.hasWeather) g_logFile.printf("# weather: %s  air_C: %.1f\n", deploy.weather, deploy.airTemp);
    g_logFile.printf("# notes: %s\n", deploy.notes);
  } else {
    g_logFile.printf("# deployment: (none entered)\n");
  }
  g_logFile.printf("# cal_ph: %s  cal_ec: %s  cal_orp: %s  cal_cyc: %s\n",
                   cal.ph_valid ? "Y" : "N", cal.ec_valid ? "Y" : "N", cal.orp_valid ? "Y" : "N", cal.cyc_valid ? "Y" : "N");
  if (deploy.cyc_en) g_logFile.printf("# cyclops_units: %s\n", deploy.cyc_units);
  g_logFile.println("ms,utc,submerged,poi,P_mbar,depth_m,bar30T_C,poetT_mC,ugs_uV,orp_uV,ec_nA,ec_uV,pH,EC_mScm,sal_PSU,ORP_Eh_mV,cyc_V,cyc_conc");
  g_logFile.flush();
}

static void openDiveLog() {
  if (!g_sdReady) return;
  nextLogPath(g_logPath, sizeof(g_logPath));
  g_logFile = SD.open(g_logPath, FILE_WRITE);
  if (g_logFile) { deploy.castNum++; writeMetaHeader(); g_logging = true;
    Serial.print("Logging to "); Serial.println(g_logPath); }
}
static void closeDiveLog() {
  if (g_logFile) { g_logFile.flush(); g_logFile.close(); }
  g_logging = false; Serial.println("Log closed (surfaced)");
}

static void writeLogRow(bool poi) {
  if (!g_logging || !g_logFile) return;
  float pH  = phFromUgs(g_poet.ugs_uV, g_poet.temp_mC / 1000.0f);
  float ec  = conductivity_mS(g_poet.ec_uV, g_poet.ec_nA);
  float sal = salinityPSU(ec, g_poet.temp_mC / 1000.0f, g_P);
  float eh  = orpEh_mV(g_poet.orp_uV);
  char ts[24]; isoTime(nowUnix(), ts, sizeof(ts));

  g_logFile.printf("%lu,%s,%d,%d,%.1f,%.3f,%.2f,%ld,%ld,%ld,%ld,%ld,",
    (unsigned long)millis(), ts, g_submerged ? 1 : 0, poi ? 1 : 0,
    g_P, g_depth, g_barT, (long)g_poet.temp_mC, (long)g_poet.ugs_uV,
    (long)g_poet.orp_uV, (long)g_poet.ec_nA, (long)g_poet.ec_uV);
  if (isnan(pH))  g_logFile.print(""); else g_logFile.print(pH, 3);  g_logFile.print(',');
  if (isnan(ec))  g_logFile.print(""); else g_logFile.print(ec, 3);  g_logFile.print(',');
  if (isnan(sal)) g_logFile.print(""); else g_logFile.print(sal, 3); g_logFile.print(',');
  if (isnan(eh))  g_logFile.print(""); else g_logFile.print(eh, 1);  g_logFile.print(',');
  // Cyclops: raw volts then calibrated concentration (blank when no ADC / not enabled)
  float cc = cycConc(g_cycV);
  if (g_cycOk && deploy.cyc_en) g_logFile.print(g_cycV, 4); g_logFile.print(',');
  if (g_cycOk && deploy.cyc_en && !isnan(cc)) g_logFile.println(cc, 3); else g_logFile.println("");
  g_logFile.flush();
}

// ---------------- submersion gate ----------------
static void updateLoggingGate() {
  if (g_submerged) { g_subStreak++; g_surfStreak = 0; } else { g_surfStreak++; g_subStreak = 0; }
  if (!g_logging && g_submerged && g_subStreak >= SUBMERGE_DEBOUNCE) openDiveLog();
  if (g_logging && !g_submerged && g_surfStreak >= SURFACE_DEBOUNCE) closeDiveLog();
}

// ---------------- run-mode UI (portrait 240x320) ----------------
static void footer() {
  tft.setTextSize(1); tft.setCursor(4, SCR_H - 12);
  tft.setTextColor(g_sdReady ? ST77XX_GREEN : ST77XX_RED); tft.print(g_sdReady ? "SD " : "SD!");
  tft.setTextColor(g_logging ? ST77XX_GREEN : ST77XX_YELLOW); tft.print(g_logging ? " LOG " : " IDLE ");
  tft.setTextColor(g_timeSynced ? (g_timeApprox ? ST77XX_YELLOW : ST77XX_GREEN) : ST77XX_RED);
  tft.print(g_timeSynced ? (g_timeApprox ? "t~ " : "t " ) : "t! ");
  tft.setTextColor(ST77XX_WHITE);
  tft.print("POI:"); tft.print(g_poiCount); tft.print(" n:"); tft.print(g_sampleCount);
  tft.setTextColor(g_submerged ? ST77XX_CYAN : ST77XX_RED); tft.print(g_submerged ? " WET" : " AIR");
}

// ---- shared tile primitive: threshold colour-coded, themed OK colour ----
TileState tileState(MetricId m, float v) {
  if (isnan(v)) return T_OK;                       // unknown -> neutral, never a false alarm
  Threshold &t = deploy.thresh[m];
  if ((!isnan(t.alarmLo) && v < t.alarmLo) || (!isnan(t.alarmHi) && v > t.alarmHi)) return T_ALARM;
  if ((!isnan(t.warnLo)  && v < t.warnLo)  || (!isnan(t.warnHi)  && v > t.warnHi))  return T_WARN;
  return T_OK;
}

void drawTile(int16_t x, int16_t y, int16_t w, int16_t h,
              const char *label, const char *val, const char *units, TileState st, uint8_t maxVs) {
  uint16_t edge, ink, bg, sub;
  switch (st) {
    case T_ALARM: bg = ST77XX_RED;   edge = ST77XX_RED; ink = ST77XX_BLACK; sub = ST77XX_BLACK; break; // solid / negative
    case T_WARN:  bg = ST77XX_BLACK; edge = COL_WARN;   ink = COL_WARN;     sub = COL_LABEL;    break;
    default:      bg = ST77XX_BLACK; edge = COL_BORDER; ink = g_accent;     sub = COL_LABEL;    break; // outlined, themed
  }
  tft.fillRoundRect(x, y, w, h, 6, bg);
  tft.drawRoundRect(x, y, w, h, 6, edge);
  if (st == T_WARN) tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, edge);   // thicker ring

  tft.setTextSize(1); tft.setTextColor(sub);
  tft.setCursor(x + (w - (int)strlen(label) * 6) / 2, y + 9); tft.print(label);

  uint8_t vs = maxVs;                                      // shrink to fit if the value is wide
  int vw = strlen(val) * 6 * vs;
  while (vw > w - 6 && vs > 1) { vs--; vw = strlen(val) * 6 * vs; }
  tft.setTextSize(vs); tft.setTextColor(ink);
  tft.setCursor(x + (w - vw) / 2, y + (h - vs * 8) / 2); tft.print(val);

  tft.setTextSize(1); tft.setTextColor(sub);
  tft.setCursor(x + (w - (int)strlen(units) * 6) / 2, y + h - 14); tft.print(units);
}

// ---------------- OTA progress screens ----------------
// Drawn from inside the upload handler while loop() is blocked for the whole transfer.
// Mostly-black (low panel current, brownout-friendly) but legible so the user reads the warning.
static const int OTA_BX = 14, OTA_BY = 150, OTA_BW = 212, OTA_BH = 22;   // progress-bar frame

void otaScreenBegin() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3); tft.setTextColor(g_accent);
  tft.setCursor(20, 50); tft.print("UPDATING");
  tft.setTextSize(2); tft.setTextColor(ST77XX_RED);
  tft.setCursor(8, 92);  tft.print("DO NOT");
  tft.setCursor(8, 114); tft.print("POWER OFF");
  tft.drawRoundRect(OTA_BX, OTA_BY, OTA_BW, OTA_BH, 4, COL_BORDER);
}

void otaScreenBar(uint8_t pct) {
  if (pct > 100) pct = 100;
  tft.fillRoundRect(OTA_BX + 2, OTA_BY + 2, ((OTA_BW - 4) * pct) / 100, OTA_BH - 4, 3, g_accent);
  tft.fillRect(96, OTA_BY + OTA_BH + 10, 60, 18, ST77XX_BLACK);   // clear old %
  char p[8]; snprintf(p, sizeof(p), "%u%%", pct);
  tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(100, OTA_BY + OTA_BH + 10); tft.print(p);
}

// Terminal OTA message (success/failure). l2 uses small text so long error strings still fit.
void otaScreenMsg(const char *l1, const char *l2) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2); tft.setTextColor(g_accent);
  tft.setCursor(10, 60); tft.print(l1);
  if (l2 && *l2) {
    tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 96); tft.print(l2);
  }
}

// page 0: dive computer  (1 depth hero + ascent/temp + pH/sal)
static void divePage() {
  tft.fillScreen(ST77XX_BLACK);
  bool live = g_submerged && g_poetOk;
  float depth = g_submerged ? g_depth : NAN;
  float ph  = live ? phFromUgs(g_poet.ugs_uV, g_poet.temp_mC / 1000.0f) : NAN;
  float sal = live ? salinityPSU(conductivity_mS(g_poet.ec_uV, g_poet.ec_nA), g_poet.temp_mC / 1000.0f, g_P) : NAN;
  char buf[12];

  // hero: depth (full width).  Out of water -> "--" rather than a misleading 0.0
  if (isnan(depth)) strcpy(buf, "--"); else snprintf(buf, sizeof(buf), "%.1f", depth);
  drawTile(2, 2, 236, 96, "DEPTH", buf, "m", tileState(M_DEPTH, depth), 6);

  // ascent rate (danger = ascending too fast; g_ascent < 0 means coming up).  "--" out of water.
  TileState aSt = T_OK;
  const char *dir = "RISE";
  if (g_submerged) {
    if (g_ascent < -10) aSt = T_ALARM; else if (g_ascent < -8) aSt = T_WARN;
    dir = (g_ascent <= -0.1f) ? "UP" : (g_ascent >= 0.1f ? "DOWN" : "HOLD");
    snprintf(buf, sizeof(buf), "%.1f", fabs(g_ascent));
  } else {
    strcpy(buf, "--");
  }
  drawTile(2, 100, 117, 98, dir, buf, "m/min", aSt, 5);

  snprintf(buf, sizeof(buf), "%.1f", g_barT);
  drawTile(121, 100, 117, 98, "TEMP", buf, "C", tileState(M_TEMP, g_barT));

  if (isnan(ph)) strcpy(buf, "--"); else snprintf(buf, sizeof(buf), "%.2f", ph);
  drawTile(2, 200, 117, 98, "pH", buf, "", tileState(M_PH, ph));

  if (isnan(sal)) strcpy(buf, "--"); else snprintf(buf, sizeof(buf), "%.1f", sal);
  drawTile(121, 200, 117, 98, "SAL", buf, "PSU", tileState(M_SAL, sal));

  footer();
}

// page 1: metrics  (2 cols x 3 rows)
static void waterPage() {
  tft.fillScreen(ST77XX_BLACK);
  bool live = g_submerged && g_poetOk;
  float Tc  = live ? g_poet.temp_mC / 1000.0f : NAN;
  float ecR = live ? conductivity_mS(g_poet.ec_uV, g_poet.ec_nA) : NAN;
  float ph  = live ? phFromUgs(g_poet.ugs_uV, Tc) : NAN;
  float ec  = live ? specCond25_mS(ecR, Tc) : NAN;
  float sal = live ? salinityPSU(ecR, Tc, g_P) : NAN;
  float eh  = live ? orpEh_mV(g_poet.orp_uV) : NAN;
  float dep = g_submerged ? g_depth : NAN;   // out of water -> "--" (loop handles NaN)

  struct { const char *lab; MetricId id; float v; const char *un; uint8_t dp; } T[6] = {
    {"TEMP",  M_TEMP,  Tc,  "C",     1},
    {"pH",    M_PH,    ph,  "",      2},
    {"ORP",   M_ORP,   eh,  "mV",    0},
    {"EC",    M_EC,    ec,  "mS/cm", 1},
    {"SAL",   M_SAL,   sal, "PSU",   1},
    {"DEPTH", M_DEPTH, dep, "m",     1},
  };

  // 6th tile shows the Cyclops fluorometer when fitted (depth still leads the dive page);
  // calibrated -> concentration in the chosen units, uncalibrated -> raw volts.
  if (deploy.cyc_en) {
    bool cald = cal.cyc_valid;
    float cv = g_cycOk ? (cald ? cycConc(g_cycV) : g_cycV) : NAN;
    T[5].lab = "FLUOR"; T[5].id = M_CYC; T[5].v = cv;
    T[5].un = cald ? deploy.cyc_units : "V"; T[5].dp = cald ? 1 : 2;
  }

  const int gap = 2, top = 2, tw = 117, th = 98;
  char buf[12];
  for (int i = 0; i < 6; i++) {
    int x = gap + (i % 2) * (tw + gap);   // 2 / 121
    int y = top + (i / 2) * (th + gap);   // 2 / 102 / 202
    if (isnan(T[i].v)) strcpy(buf, "--");
    else snprintf(buf, sizeof(buf), "%.*f", T[i].dp, T[i].v);
    drawTile(x, y, tw, th, T[i].lab, buf, T[i].un, tileState(T[i].id, T[i].v));
  }
  footer();
}

void renderRun() {
  if (g_page == 0) divePage(); else waterPage();
  if (millis() < g_poiBannerUntil) {                       // centred banner overlay
    bool poi = (g_bannerKind == 0);
    tft.fillRoundRect(20, 132, 200, 56, 8, poi ? ST77XX_BLUE : COL_BORDER);
    tft.drawRoundRect(20, 132, 200, 56, 8, ST77XX_WHITE);
    char m[16]; uint8_t ts = poi ? 3 : 2; int cw = poi ? 18 : 12;
    if (poi) snprintf(m, sizeof(m), "POI #%lu", (unsigned long)g_poiCount);
    else     strcpy(m, "NOT LOGGING");
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(ts);
    tft.setCursor(20 + (200 - (int)strlen(m) * cw) / 2, 132 + (56 - ts * 8) / 2); tft.print(m);
  }
}

void runHandleNav(NavEvent e) {
  if (e == NAV_SHORT) {                 // POI when logging, else just acknowledge the press
    g_poiBannerUntil = millis() + POI_BANNER_MS;
    if (g_logging) { g_poiCount++; writeLogRow(true); g_bannerKind = 0;
      Serial.print("POI #"); Serial.println(g_poiCount); }
    else { g_bannerKind = 1; Serial.println("short press (not logging)"); }
    renderRun();
  } else if (e == NAV_LONG) {           // page flip
    g_page ^= 1; renderRun();
  }
}

// ---------------- button -> NavEvent (responsive: long fires at threshold, short on release) ----------------
static void buttonPoll() {
  static bool rawLast = false, stable = false;
  static uint32_t tEdge = 0, tDown = 0;
  static bool longSent = false;
  bool raw = (digitalRead(PIN_BUTTON) == LOW);     // active-low (provisional)
  if (raw != rawLast) { rawLast = raw; tEdge = millis(); }
  if (millis() - tEdge > BTN_DEBOUNCE_MS && raw != stable) {
    stable = raw;
    if (stable) { tDown = millis(); longSent = false; }
    else if (!longSent && (millis() - tDown) >= BTN_DEBOUNCE_MS) g_nav = NAV_SHORT;
  }
  if (stable && !longSent && (millis() - tDown) >= BTN_LONG_MS) { longSent = true; g_nav = NAV_LONG; }
}

static void drawStatus(const char *l1, const char *l2 = "", const char *l3 = "") {
  tft.fillScreen(ST77XX_BLACK); tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
  tft.setCursor(6, 12); tft.println(l1);
  if (strlen(l2)) { tft.setCursor(6, 44); tft.println(l2); }
  if (strlen(l3)) { tft.setCursor(6, 76); tft.println(l3); }
}

// quick I2C presence probe: addresses the device and checks for an ACK.
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Boot self-test: probe each sensor's I2C address and report OK / FAILED on screen.
// Required sensors (POET, BAR30) read green/red; the optional Cyclops ADC reads
// green when present, red only if the mission has it enabled but it's missing,
// otherwise a muted "off" so an un-fitted fluorometer never looks like a fault.
static void drawSensorScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2); tft.setTextColor(g_accent);
  tft.setCursor(6, 10); tft.println("SENSORS");

  struct { const char *name; uint8_t addr; bool required; } S[] = {
    { "POET",  POET_ADDR,  true  },
    { "BAR30", BAR30_ADDR, true  },
    { "CYCLOPS", ADS_ADDR, false },
  };

  int y = 52;
  for (auto &s : S) {
    bool ok = i2cPresent(s.addr);
    uint16_t col; const char *stat;
    if (ok)                 { col = ST77XX_GREEN;  stat = "OK"; }
    else if (s.required)    { col = ST77XX_RED;    stat = "FAILED"; }
    else if (deploy.cyc_en) { col = ST77XX_RED;    stat = "FAILED"; }   // declared fitted but absent
    else                    { col = COL_LABEL;     stat = "off"; }      // optional, not fitted
    char line[28];
    snprintf(line, sizeof(line), "%s - %s", s.name, stat);
    tft.setTextSize(2); tft.setTextColor(col);
    tft.setCursor(6, y); tft.println(line);
    y += 32;
  }
}

// safe per-mission defaults; state.json (if present) and the portal override these
static void thresholdsDefault() {
  for (int i = 0; i < M_COUNT; i++) deploy.thresh[i] = { NAN, NAN, NAN, NAN };  // all disabled
  deploy.thresh[M_PH]    = { 7.8f, 8.4f, 7.5f, 8.6f };   // placeholder bands
  deploy.thresh[M_DEPTH] = { NAN,  30.0f, NAN, 40.0f };
}

// Boot-hold twist gesture, shown on screen with a progress bar. Three outcomes from one button:
//   release during the CAL window      -> BOOT_NORMAL
//   held through the CAL window, release-> BOOT_CAL
//   KEEP holding past CAL too           -> BOOT_RECOVERY (upload-only firmware AP)
// The CAL window samples the button and accepts a majority-LOW hold so the twist actuator can
// briefly chatter open mid-hold. Recovery is the sealed-unit escape hatch, so it is decided here
// BEFORE any sensor/SD init -- a faulty driver must never be able to block re-flashing.
static BootMode bootHoldGesture() {
  if (digitalRead(PIN_BUTTON) != LOW) return BOOT_NORMAL;  // not held at power-on -> normal boot

  // ---- phase A: CAL window ----
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2); tft.setTextColor(g_accent);
  tft.setCursor(34, 86); tft.print("HOLD = CAL");
  tft.setTextSize(1); tft.setTextColor(COL_LABEL);
  tft.setCursor(34, 118); tft.print("release to boot normally");
  const int bx = 20, by = 162, bw = 200, bh = 20;
  tft.drawRoundRect(bx, by, bw, bh, 4, COL_BORDER);

  int lo = 0, tot = 0; uint32_t t0 = millis();
  while (millis() - t0 < BTN_BOOTHOLD_MS) {
    if (digitalRead(PIN_BUTTON) == LOW) lo++;
    tot++;
    int pct = (int)((millis() - t0) * 100 / BTN_BOOTHOLD_MS);
    tft.fillRoundRect(bx + 2, by + 2, ((bw - 4) * pct) / 100, bh - 4, 3, g_accent);
    delay(20);
  }
  if (!(tot && (lo * 100 / tot >= 60))) {                  // released early -> normal boot
    uint32_t t1 = millis();
    while (digitalRead(PIN_BUTTON) == LOW && millis() - t1 < 4000) delay(5);   // drain
    return BOOT_NORMAL;
  }

  // ---- phase B: keep holding -> RECOVERY, release now -> CAL ----
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2); tft.setTextColor(ST77XX_RED);
  tft.setCursor(8, 70);  tft.print("KEEP HOLDING");
  tft.setTextColor(g_accent);
  tft.setCursor(8, 98);  tft.print("= RECOVERY");
  tft.setTextSize(1); tft.setTextColor(COL_LABEL);
  tft.setCursor(8, 130); tft.print("release now to calibrate");
  tft.drawRoundRect(bx, by, bw, bh, 4, COL_BORDER);

  uint32_t t2 = millis();
  while (millis() - t2 < BTN_RECOVERY_MS) {
    if (digitalRead(PIN_BUTTON) != LOW) return BOOT_CAL;   // let go in the recovery window -> CAL
    int pct = (int)((millis() - t2) * 100 / BTN_RECOVERY_MS);
    tft.fillRoundRect(bx + 2, by + 2, ((bw - 4) * pct) / 100, bh - 4, 3, ST77XX_RED);
    delay(20);
  }
  uint32_t t3 = millis();                                  // held the whole window -> recovery
  while (digitalRead(PIN_BUTTON) == LOW && millis() - t3 < 6000) delay(5);   // drain
  return BOOT_RECOVERY;
}

// ---------------- setup ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  backlightBegin();            // PWM on the backlight (full brightness); enables auto-dim later
  pinMode(PIN_TFT_CS, OUTPUT); digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);  digitalWrite(PIN_SD_CS, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  Wire.begin(PIN_SDA, PIN_SCL);
  thresholdsDefault();

  delay(150); digitalWrite(PIN_TFT_RST, HIGH); pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, LOW); delay(20); digitalWrite(PIN_TFT_RST, HIGH); delay(150);
  tft.init(240, 320); tft.setRotation(TFT_ROT); tft.fillScreen(ST77XX_BLACK);

  BootMode bm = bootHoldGesture();     // hold actuator at power-on, with on-screen feedback

  if (bm == BOOT_RECOVERY) {           // sealed-unit escape hatch: upload-only firmware AP.
    g_recovery = true;                 // decided before ANY sensor/SD init so a bad driver can't block it
    portalBeginRecovery();
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2); tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 18); tft.println("RECOVERY");
    tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 58); tft.println("Wi-Fi: WaterQuality-Logger");
    tft.setCursor(10, 76); tft.println("open  192.168.4.1");
    tft.setCursor(10, 94); tft.println("upload firmware .bin");
    tft.setTextColor(COL_LABEL);
    tft.setCursor(10, 120); tft.println("power-cycle to leave");
    return;                            // loop() services only the upload AP from here
  }
  bool wantCal = (bm == BOOT_CAL);

  drawStatus("ST7789 OK", wantCal ? "-> CALIBRATE" : "");
  g_sdReady = sdMount();
  if (g_sdReady) {
    char l2[24]; snprintf(l2, sizeof(l2), "SD:%s %luMB", sdTypeName(),
      (unsigned long)(SD.cardSize() / (1024ULL * 1024ULL)));
    drawStatus("ST7789 OK", l2, wantCal ? "-> CALIBRATE" : "");
    calLoad();      // load cal.json
    stateLoad();    // load epoch + thresholds + accent (best-effort)
  } else drawStatus("ST7789 OK", "SD: NO CARD", wantCal ? "-> CALIBRATE" : "");
  g_accent = accentColor(deploy.accent);
  delay(900);

  if (bar30.init()) { bar30.setModel(MS5837::MS5837_30BA); bar30.setFluidDensity(997); }
  else Serial.println("Bar30 init failed");

  g_cycOk = ads.begin(ADS_ADDR, &Wire);          // Cyclops ADC (optional)
  if (g_cycOk) { ads.setGain(GAIN_ONE); Serial.println("ADS1015 (Cyclops) ready"); }
  else Serial.println("ADS1015 not found (Cyclops disabled)");

  drawSensorScreen();      // boot self-test: POET / BAR30 / CYCLOPS I2C presence
  delay(1800);

  if (wantCal) { g_mode = MODE_CAL; calBegin(); }
  else {
    g_mode = MODE_RUN; portalBegin();
    drawStatus("Setup WiFi:", "WaterQuality-Logger", "open 192.168.4.1");
    delay(1800);
  }

  g_lastSampleMs = millis();
  if (g_mode == MODE_RUN) renderRun(); else calRender();
}

// ---------------- loop ----------------
enum { S_IDLE, S_POET_WAIT };
static uint8_t  g_state = S_IDLE;
static uint32_t g_poetT0 = 0;

static void sampleFinish() {
  bar30.read();
  g_P = bar30.pressure(); g_barT = bar30.temperature(); g_depth = bar30.depth();
  g_poetOk = poetFetch(g_poet);
  g_submerged = g_poetOk && (g_poet.ec_nA > EC_SUBMERGED_NA);
  if (g_cycOk) g_cycV = ads.computeVolts(ads.readADC_SingleEnded(CYC_ADC_CH)) * CYC_DIVIDER;  // 0-5 V at sensor

  uint32_t now = millis(); float dt = (now - g_lastSampleMs) / 1000.0f;
  if (dt > 0.1f) g_ascent = (g_depth - g_lastDepth) / dt * 60.0f;
  g_lastDepth = g_depth; g_lastSampleMs = now; g_sampleCount++;

  if (g_mode == MODE_RUN) {
    updateLoggingGate();
    writeLogRow(false);
    // tear down WiFi once we go underwater (useless + saves power)
    if (g_logging && portalActive()) portalEnd();   // keep AP up on surface; drop only once a dive starts
    renderRun();
  } else {
    calOnSample();
    calRender();
  }
}

void loop() {
  // A finished OTA asks for a reboot here (not inside the handler) so the HTTP reply flushes first.
  if (g_otaRebootAt && (int32_t)(millis() - g_otaRebootAt) >= 0) { delay(50); ESP.restart(); }
  // Recovery mode is upload-only: service just the AP (which carries /api/ota); no sampling/UI.
  if (g_recovery) { portalLoop(); return; }

  buttonPoll();
  if (portalActive()) portalLoop();

  NavEvent e = g_nav; g_nav = NAV_NONE;
  if (e != NAV_NONE) {
    if (backlightWake()) {
      // Screen was dimmed/off: this twist only wakes it -- swallow it (no POI, no page flip).
    } else if (g_mode == MODE_CAL) {
      calHandleNav(e);
    } else {
      runHandleNav(e);
    }
  }

  if (g_reqCal) {                       // portal asked to enter calibration
    g_reqCal = false;
    if (portalActive()) portalEnd();
    g_mode = MODE_CAL; calBegin(); calRender();
  }
  if (g_uiDirty && g_mode == MODE_RUN) { g_uiDirty = false; renderRun(); }  // e.g. accent changed

  backlightLoop();                      // advance the idle->fade->off timer

  switch (g_state) {
    case S_IDLE:
      if (millis() - g_lastSampleMs >= SAMPLE_MS) { if (poetStart()) { g_poetT0 = millis(); g_state = S_POET_WAIT; } }
      break;
    case S_POET_WAIT:
      if (millis() - g_poetT0 >= POET_WAIT_MS) { sampleFinish(); g_state = S_IDLE; }
      break;
  }

  static uint32_t lastBanner = 0;
  if (g_poiBannerUntil && millis() > g_poiBannerUntil && lastBanner != g_poiBannerUntil) {
    lastBanner = g_poiBannerUntil; if (g_mode == MODE_RUN) renderRun();
  }
}
