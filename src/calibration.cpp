#include "shared.h"

// ============================================================
//  Conversions (return NAN when the relevant sensor is uncalibrated)
// ============================================================
float phFromUgs(int32_t ugs_uV, float T) {
  if (!cal.ph_valid) return NAN;
  float ugs = ugs_uV / 1000.0f;
  // Nernstian temperature compensation of the slope (slope ~ absolute T)
  float slope = cal.ph_slope * (T + 273.15f) / (cal.ph_calT + 273.15f);
  if (fabs(slope) < 1e-3f) return NAN;
  return 7.00f + (ugs - cal.ph_ugs7_mV) / slope;
}

float conductivity_mS(int32_t ec_uV, int32_t ec_nA) {     // in-situ conductivity, mS/cm
  if (!cal.ec_valid || ec_nA == 0) return NAN;
  float R = (float)ec_uV / ec_nA * 1000.0f;               // ohms (kelvin 4-terminal)
  if (R <= 0) return NAN;
  return cal.ec_K / R * 1000.0f;                          // (K/R) S/cm -> mS/cm
}

float specCond25_mS(float c_mS, float T) {                // specific conductance @25C
  if (isnan(c_mS)) return NAN;
  return c_mS / (1.0f + 0.02f * (T - 25.0f));             // ~2%/C natural water
}

float orpEh_mV(int32_t orp_uV) {
  if (!cal.orp_valid) return NAN;
  return orp_uV / 1000.0f + cal.orp_offset_mV;
}

// UNESCO 1983 / PSS-78 practical salinity from in-situ C (mS/cm), T (C), P (dbar)
static double sal78(double C, double T, double P) {
  double R = C / C_STD_SEAWATER;
  double rt = 0.6766097 + 2.00564e-2*T + 1.104259e-4*T*T - 6.9698e-7*T*T*T + 1.0031e-9*T*T*T*T;
  double a1=2.07e-5, a2=-6.37e-10, a3=3.989e-15;
  double b1=3.426e-2, b2=4.464e-4, b3=4.215e-1, b4=-3.107e-3;
  double Rp = 1.0 + (P*(a1 + a2*P + a3*P*P)) / (1.0 + b1*T + b2*T*T + (b3 + b4*T)*R);
  double Rt = R / (Rp*rt), s = sqrt(Rt);
  double a[6]={0.0080,-0.1692,25.3851,14.0941,-7.0261,2.7081};
  double b[6]={0.0005,-0.0056,-0.0066,-0.0375,0.0636,-0.0144};
  double S=0, B=0, p=1.0;
  for (int i=0;i<6;i++){ S+=a[i]*p; B+=b[i]*p; p*=s; }
  S += (T-15.0)/(1.0+0.0162*(T-15.0)) * B;
  return S;
}
float salinityPSU(float c_mS, float T, float P_mbar) {
  if (isnan(c_mS) || c_mS <= 0) return NAN;
  double dbar = (P_mbar - 1013.25) / 100.0; if (dbar < 0) dbar = 0;
  double s = sal78(c_mS, T, dbar);
  if (s < 0 || s > 45) return NAN;        // outside PSS-78 valid range
  return (float)s;
}

float cycConc(float volts) {              // Cyclops direct-concentration: conc = k*(V - blank)
  if (!cal.cyc_valid) return NAN;
  return cal.cyc_k * (volts - cal.cyc_v0);
}

// ============================================================
//  cal.json + callog.csv
//  calLoad reads cal.json into `cal`.  calSave writes `cal` to cal.json and is called
//  ONLY on an explicit SAVE & EXIT.  Capture steps stage into `calStaged`, never `cal`.
// ============================================================
static CalData calStaged;          // working copy; committed to `cal` only on save & exit

void calLoad() {
  if (!SD.exists("/cal.json")) return;
  File f = SD.open("/cal.json", FILE_READ); if (!f) return;
  JsonDocument d; if (deserializeJson(d, f)) { f.close(); return; } f.close();
  cal.ph_valid  = d["ph"]["valid"]  | false; cal.ph_ugs7_mV = d["ph"]["ugs7"] | 0.0f;
  cal.ph_slope  = d["ph"]["slope"]  | 0.0f;  cal.ph_calT    = d["ph"]["calT"] | 25.0f;
  cal.ec_valid  = d["ec"]["valid"]  | false; cal.ec_K       = d["ec"]["K"]    | 0.0f;
  cal.ec_calT   = d["ec"]["calT"]   | 25.0f;
  cal.orp_valid = d["orp"]["valid"] | false; cal.orp_offset_mV = d["orp"]["offset"] | 0.0f;
  cal.cyc_valid = d["cyc"]["valid"] | false; cal.cyc_v0 = d["cyc"]["v0"] | 0.0f; cal.cyc_k = d["cyc"]["k"] | 0.0f;
  cal.ph_epoch  = d["ph"]["epoch"]  | 0UL;
  cal.ec_epoch  = d["ec"]["epoch"]  | 0UL;
  cal.orp_epoch = d["orp"]["epoch"] | 0UL;
  cal.cyc_epoch = d["cyc"]["epoch"] | 0UL;
  Serial.println("cal.json loaded");
}

static bool calSave() {            // writes the live `cal` struct; called only from SAVE & EXIT
  if (!sdEnsure()) { Serial.println("cal.json: SD not ready"); return false; }
  JsonDocument d;
  JsonObject ph = d["ph"].to<JsonObject>();
  ph["valid"]=cal.ph_valid; ph["ugs7"]=cal.ph_ugs7_mV; ph["slope"]=cal.ph_slope; ph["calT"]=cal.ph_calT; ph["epoch"]=cal.ph_epoch;
  JsonObject ec = d["ec"].to<JsonObject>();
  ec["valid"]=cal.ec_valid; ec["K"]=cal.ec_K; ec["calT"]=cal.ec_calT; ec["epoch"]=cal.ec_epoch;
  JsonObject orp = d["orp"].to<JsonObject>();
  orp["valid"]=cal.orp_valid; orp["offset"]=cal.orp_offset_mV; orp["epoch"]=cal.orp_epoch;
  JsonObject cyc = d["cyc"].to<JsonObject>();
  cyc["valid"]=cal.cyc_valid; cyc["v0"]=cal.cyc_v0; cyc["k"]=cal.cyc_k; cyc["epoch"]=cal.cyc_epoch;
  if (SD.exists("/cal.json")) SD.remove("/cal.json");
  File f = SD.open("/cal.json", FILE_WRITE);
  if (!f) { Serial.println("cal.json: OPEN FAILED"); return false; }
  size_t n = serializeJsonPretty(d, f);
  f.flush(); f.close();
  bool ok = (n > 0) && SD.exists("/cal.json");
  Serial.printf("cal.json write %s (%u bytes)\n", ok ? "OK" : "FAIL", (unsigned)n);
  return ok;
}

static void calLogAppend(const char *sensor, const char *standard,
                         const char *raw, const char *coeff, float calT) {
  if (!sdEnsure()) return;
  bool fresh = !SD.exists("/callog.csv");
  File f = SD.open("/callog.csv", FILE_APPEND); if (!f) return;
  if (fresh) f.println("utc,sensor,standard,raw,coeff,calT_C,note");
  char ts[24]; isoTime(nowUnix(), ts, sizeof(ts));
  f.printf("%s,%s,%s,%s,%s,%.2f,\n", ts, sensor, standard, raw, coeff, calT);
  f.close();
}

// ============================================================
//  Wizard
// ============================================================
enum CalStep {
  CS_MENU,
  CS_PH_R7, CS_PH_M7, CS_PH_R4, CS_PH_M4, CS_PH_R10, CS_PH_M10,
  CS_EC_SEL, CS_EC_MEAS,
  CS_ORP_SEL, CS_ORP_MEAS,
  CS_CYC_RB, CS_CYC_MB, CS_CYC_RS, CS_CYC_MS,
  CS_SAVE, CS_QUIT
};
static CalStep s_step = CS_MENU;
static int     s_menu = 0;
static const char *MENU[] = {"pH (3-pt)", "EC (1-pt)", "ORP (1-pt)", "Cyclops (2-pt)", "SAVE & EXIT", "EXIT no save"};
static const int  MENU_N = 6;
static const int  MI_SAVE = 4, MI_QUIT = 5;     // exit item indices

// preset standards (edit to match what you actually own)
static const long  EC_STD_uS[]  = {1413, 12880, 80000};   // Atlas K1.0 set = 12880 & 80000
static const char *EC_STD_LBL[] = {"1413 uS", "12.88 mS", "80.0 mS"};
static int s_ecSel = 2;                                    // 80 mS ~ closest to seawater
static const long  ORP_STD_mV[] = {240, 470};
static const char *ORP_STD_LBL[] = {"240 mV", "470 mV"};
static int s_orpSel = 0;

// captured intermediates
static float s_ugs7, s_calT7, s_ugs4, s_calT4, s_ugs10, s_calT10, s_ecR, s_orpRaw;
static float s_cycV0, s_cycVs;     // Cyclops blank + standard volts
static char  s_saveMsg[28] = "";   // staged-status line shown on the menu

// rolling stability buffer on the active channel
#define STAB_N 6
static float s_hist[STAB_N]; static int s_histCnt = 0; static int s_histIdx = 0;
static float s_stabThresh = 2.0f;                // set per step (mV or %)

static void stabReset(float thresh) { s_histCnt = 0; s_histIdx = 0; s_stabThresh = thresh; }
static void stabPush(float v) { s_hist[s_histIdx] = v; s_histIdx = (s_histIdx + 1) % STAB_N; if (s_histCnt < STAB_N) s_histCnt++; }
static float stabSpread() {
  if (s_histCnt < STAB_N) return 1e9;
  float lo = s_hist[0], hi = s_hist[0];
  for (int i = 1; i < s_histCnt; i++) { lo = min(lo, s_hist[i]); hi = max(hi, s_hist[i]); }
  return hi - lo;
}
static bool stabOk() { return stabSpread() <= s_stabThresh; }
static int  stabPct() { float sp = stabSpread(); if (sp >= 1e8) return 0; int p = 100 - (int)(sp / s_stabThresh * 100); return constrain(p, 0, 100); }

void calBegin() {
  s_step = CS_MENU; s_menu = 0; stabReset(2.0f);
  calStaged = cal;                  // start from existing calibration; edits stay staged
  snprintf(s_saveMsg, sizeof(s_saveMsg), "no changes staged");
  if (!sdEnsure()) Serial.println("WARNING: SD not ready -- calibration cannot be saved");
  Serial.println("=== CALIBRATION MODE (short=move/capture, long=select/cancel; nothing saved until SAVE & EXIT) ===");
}

// push the channel relevant to the current step into the stability buffer
void calOnSample() {
  if (s_step == CS_CYC_MB || s_step == CS_CYC_MS) {     // Cyclops uses the ADC, not the POET
    if (g_cycOk) stabPush(g_cycV);
    return;
  }
  if (!g_poetOk) return;
  switch (s_step) {
    case CS_PH_M7: case CS_PH_M4: case CS_PH_M10: stabPush(g_poet.ugs_uV / 1000.0f); break; // mV
    case CS_EC_MEAS: if (g_poet.ec_nA) stabPush((float)g_poet.ec_uV / g_poet.ec_nA * 1000.0f); break; // ohm
    case CS_ORP_MEAS: stabPush(g_poet.orp_uV / 1000.0f); break;                   // mV
    default: break;
  }
}

// ---- finishers write to calStaged ONLY (committed to cal on SAVE & EXIT) ----
static void finishPH() {
  const float p[3] = {7.00f, 4.00f, 10.00f};
  const float u[3] = {s_ugs7, s_ugs4, s_ugs10};
  float ubar = (u[0] + u[1] + u[2]) / 3.0f;             // pbar = 7.0, Sum(p-pbar)^2 = 18
  float num = 0; for (int i = 0; i < 3; i++) num += (p[i] - 7.00f) * (u[i] - ubar);
  float slope = num / 18.0f;                            // mV/pH (signed)
  float s_lo = (s_ugs4  - s_ugs7) / (4.00f - 7.00f);    // segment slopes -> linearity check
  float s_hi = (s_ugs10 - s_ugs7) / (10.00f - 7.00f);
  calStaged.ph_ugs7_mV = s_ugs7; calStaged.ph_slope = slope;
  calStaged.ph_calT = (s_calT7 + s_calT4 + s_calT10) / 3.0f;
  calStaged.ph_valid = (fabs(slope) > 1e-3f); calStaged.ph_epoch = nowUnix();
  char raw[64], co[64];
  snprintf(raw, sizeof(raw), "u7=%.1f;u4=%.1f;u10=%.1f", s_ugs7, s_ugs4, s_ugs10);
  snprintf(co, sizeof(co), "slope=%.2f;lo=%.2f;hi=%.2f", slope, s_lo, s_hi);
  calLogAppend("pH", "4.00/7.00/10.00", raw, co, calStaged.ph_calT);   // audit trail only
  snprintf(s_saveMsg, sizeof(s_saveMsg), "pH staged (not saved)");
  Serial.printf("pH staged: slope=%.2f mV/pH%s\n", slope,
                (fabs(slope) < 45 || fabs(slope) > 62) ? "  [CHECK slope]" : "");
}
static void finishEC() {
  double kappa = EC_STD_uS[s_ecSel] / 1e6;              // S/cm
  calStaged.ec_K = (float)(s_ecR * kappa); calStaged.ec_calT = g_poet.temp_mC / 1000.0f;
  calStaged.ec_valid = (calStaged.ec_K > 0); calStaged.ec_epoch = nowUnix();
  char raw[32], co[32];
  snprintf(raw, sizeof(raw), "R=%.1fohm", s_ecR);
  snprintf(co, sizeof(co), "K=%.4f/cm", calStaged.ec_K);
  calLogAppend("EC", EC_STD_LBL[s_ecSel], raw, co, calStaged.ec_calT);
  snprintf(s_saveMsg, sizeof(s_saveMsg), "EC staged (not saved)");
  Serial.printf("EC staged: R=%.1f ohm K=%.4f/cm\n", s_ecR, calStaged.ec_K);
}
static void finishORP() {
  calStaged.orp_offset_mV = ORP_STD_mV[s_orpSel] - s_orpRaw; calStaged.orp_valid = true; calStaged.orp_epoch = nowUnix();
  char raw[32], co[32];
  snprintf(raw, sizeof(raw), "meas=%.1fmV", s_orpRaw);
  snprintf(co, sizeof(co), "offset=%.1fmV", calStaged.orp_offset_mV);
  calLogAppend("ORP", ORP_STD_LBL[s_orpSel], raw, co, g_poet.temp_mC / 1000.0f);
  snprintf(s_saveMsg, sizeof(s_saveMsg), "ORP staged (not saved)");
  Serial.printf("ORP staged: offset=%.1f\n", calStaged.orp_offset_mV);
}
static void finishCyc() {
  // 2-point direct concentration: conc = k*(V - blank); k = Cstd/(Vstd - Vblank)
  float dv = s_cycVs - s_cycV0;
  if (deploy.cyc_std > 0 && fabs(dv) > 1e-4f) {
    calStaged.cyc_v0 = s_cycV0; calStaged.cyc_k = deploy.cyc_std / dv;
    calStaged.cyc_valid = true; calStaged.cyc_epoch = nowUnix();
    char raw[40], co[40];
    snprintf(raw, sizeof(raw), "Vblank=%.4f;Vstd=%.4f", s_cycV0, s_cycVs);
    snprintf(co, sizeof(co), "k=%.4f/V;std=%.2f", calStaged.cyc_k, deploy.cyc_std);
    calLogAppend("Cyclops", deploy.cyc_units, raw, co, g_barT);
    snprintf(s_saveMsg, sizeof(s_saveMsg), "Cyclops staged (not saved)");
    Serial.printf("Cyclops staged: v0=%.4f k=%.4f/V\n", s_cycV0, calStaged.cyc_k);
  } else {
    snprintf(s_saveMsg, sizeof(s_saveMsg), "Cyclops FAILED (set std/Vs)");
    Serial.println("Cyclops cal failed: std<=0 or Vstd==Vblank");
  }
}

void calHandleNav(NavEvent e) {
  switch (s_step) {
    case CS_MENU:
      if (e == NAV_SHORT) s_menu = (s_menu + 1) % MENU_N;
      else if (e == NAV_LONG) {
        if      (s_menu == 0) s_step = CS_PH_R7;
        else if (s_menu == 1) { s_step = CS_EC_SEL; s_ecSel = 1; }
        else if (s_menu == 2) { s_step = CS_ORP_SEL; s_orpSel = 0; }
        else if (s_menu == 3) s_step = CS_CYC_RB;   // Cyclops blank -> standard
        else if (s_menu == MI_SAVE) s_step = CS_SAVE;
        else                        s_step = CS_QUIT;
      }
      break;

    case CS_PH_R7: if (e == NAV_SHORT) { s_step = CS_PH_M7; stabReset(2.0f); } else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_PH_M7:
      if (e == NAV_SHORT) { s_ugs7 = g_poet.ugs_uV / 1000.0f; s_calT7 = g_poet.temp_mC / 1000.0f; s_step = CS_PH_R4; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_PH_R4: if (e == NAV_SHORT) { s_step = CS_PH_M4; stabReset(2.0f); } else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_PH_M4:
      if (e == NAV_SHORT) { s_ugs4 = g_poet.ugs_uV / 1000.0f; s_calT4 = g_poet.temp_mC / 1000.0f; s_step = CS_PH_R10; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_PH_R10: if (e == NAV_SHORT) { s_step = CS_PH_M10; stabReset(2.0f); } else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_PH_M10:
      if (e == NAV_SHORT) { s_ugs10 = g_poet.ugs_uV / 1000.0f; s_calT10 = g_poet.temp_mC / 1000.0f; finishPH(); s_step = CS_MENU; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;

    case CS_EC_SEL:
      if (e == NAV_SHORT) s_ecSel = (s_ecSel + 1) % 3;
      else if (e == NAV_LONG) { s_step = CS_EC_MEAS; stabReset(10.0f); } // 10 ohm spread; tune to your standard
      break;
    case CS_EC_MEAS:
      if (e == NAV_SHORT && g_poet.ec_nA) { s_ecR = (float)g_poet.ec_uV / g_poet.ec_nA * 1000.0f; finishEC(); s_step = CS_MENU; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;

    case CS_ORP_SEL:
      if (e == NAV_SHORT) s_orpSel = (s_orpSel + 1) % 2;
      else if (e == NAV_LONG) { s_step = CS_ORP_MEAS; stabReset(3.0f); }
      break;
    case CS_ORP_MEAS:
      if (e == NAV_SHORT) { s_orpRaw = g_poet.orp_uV / 1000.0f; finishORP(); s_step = CS_MENU; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;

    case CS_CYC_RB: if (e == NAV_SHORT) { s_step = CS_CYC_MB; stabReset(0.02f); } else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_CYC_MB:
      if (e == NAV_SHORT) { s_cycV0 = g_cycV; s_step = CS_CYC_RS; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_CYC_RS: if (e == NAV_SHORT) { s_step = CS_CYC_MS; stabReset(0.02f); } else if (e == NAV_LONG) s_step = CS_MENU; break;
    case CS_CYC_MS:
      if (e == NAV_SHORT) { s_cycVs = g_cycV; finishCyc(); s_step = CS_MENU; }
      else if (e == NAV_LONG) s_step = CS_MENU; break;

    case CS_SAVE: case CS_QUIT: break;
  }

  // terminal steps: commit (or discard) then reboot to RUN mode
  if (s_step == CS_SAVE) {
    cal = calStaged;                       // the ONLY place cal is overwritten
    bool ok = calSave();                   // the ONLY place cal.json is written
    calRender();
    Serial.printf("Calibration %s -> reboot\n", ok ? "SAVED" : "SAVE FAILED");
    delay(700); ESP.restart();
  } else if (s_step == CS_QUIT) {
    calRender();
    Serial.println("Calibration discarded (cal.json untouched) -> reboot");
    delay(700); ESP.restart();
  } else {
    calRender();
  }
}

// ---------------- wizard rendering (portrait 240x320, matches run-screen tiles) ----------------
static void calHeader(const char *title) {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRoundRect(2, 2, SCR_W - 4, 34, 6, COL_BORDER);
  tft.setTextSize(1); tft.setTextColor(g_accent); tft.setCursor(10, 6); tft.print("CALIBRATE");
  tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE); tft.setCursor(10, 18); tft.print(title);
  tft.fillRect(SCR_W - 16, 8, 8, 8, g_sdReady ? ST77XX_GREEN : ST77XX_RED);   // SD status dot
}

static void calHint(const char *l) {
  tft.setTextSize(1); tft.setTextColor(COL_LABEL); tft.setCursor(8, SCR_H - 12); tft.print(l);
}

// one big centred readout / prompt tile, identical look to the run-screen tiles
static void calTile(const char *lab, const char *val, const char *un, TileState st, uint8_t vs) {
  drawTile(20, 76, 200, 150, lab, val, un, st, vs);
}

static void calStabBar(int y) {
  int pct = stabPct(); bool ok = stabOk();
  uint16_t c = ok ? ST77XX_GREEN : COL_WARN;
  const int x = 20, w = 200, h = 14;
  tft.drawRoundRect(x, y, w, h, 4, COL_BORDER);
  if (pct > 0) tft.fillRoundRect(x + 2, y + 2, ((w - 4) * pct) / 100, h - 4, 3, c);
  const char *m = ok ? "STABLE" : "settling";
  tft.setTextSize(1); tft.setTextColor(c);
  tft.setCursor(x + (w - (int)strlen(m) * 6) / 2, y + h + 6); tft.print(m);
}

void calRender() {
  float Ugs = g_poet.ugs_uV / 1000.0f, ORPv = g_poet.orp_uV / 1000.0f;
  float R = g_poet.ec_nA ? (float)g_poet.ec_uV / g_poet.ec_nA * 1000.0f : 0;
  char buf[16];
  TileState mst;   // measure tile: amber while settling, theme colour once stable

  switch (s_step) {
    case CS_MENU: {
      calHeader("menu");
      for (int i = 0; i < MENU_N; i++) {
        int x = 10, y = 40 + i * 40, w = SCR_W - 20, h = 34;
        bool sel = (i == s_menu);
        bool isExit = (i >= MI_SAVE);
        uint16_t edge = sel ? g_accent : COL_BORDER;
        tft.drawRoundRect(x, y, w, h, 6, edge);
        tft.setTextSize(2); tft.setTextColor(sel ? g_accent : (isExit ? COL_LABEL : ST77XX_WHITE));
        tft.setCursor(x + 14, y + 10); tft.print(MENU[i]);
      }
      tft.setTextSize(1); tft.setTextColor(COL_LABEL);
      tft.setCursor(10, 286); tft.print(s_saveMsg);
      calHint("short = move   long = select");
      break;
    }

    // pH place-in prompts: the target buffer is the big number
    case CS_PH_R7:  calHeader("pH 7.00");  calTile("PLACE IN", "7.00",  "pH buffer", T_OK, 6); calHint("short = read   long = back"); break;
    case CS_PH_R4:  calHeader("pH 4.00");  calTile("PLACE IN", "4.00",  "pH buffer", T_OK, 6); calHint("short = read   long = back"); break;
    case CS_PH_R10: calHeader("pH 10.00"); calTile("PLACE IN", "10.00", "pH buffer", T_OK, 6); calHint("short = read   long = back"); break;

    // pH measure: live Ugs; tile takes the theme colour when the reading settles
    case CS_PH_M7: case CS_PH_M4: case CS_PH_M10:
      calHeader(s_step == CS_PH_M7 ? "pH 7.00" : (s_step == CS_PH_M4 ? "pH 4.00" : "pH 10.00"));
      mst = stabOk() ? T_OK : T_WARN;
      snprintf(buf, sizeof(buf), "%.1f", Ugs);
      calTile("Ugs", buf, "mV", mst, 6);
      calStabBar(244);
      calHint("short = capture   long = cancel");
      break;

    case CS_EC_SEL:
      calHeader("EC standard");
      calTile("EC STD", EC_STD_LBL[s_ecSel], "", T_WARN, 4);
      calHint("short = cycle   long = select");
      break;
    case CS_EC_MEAS:
      calHeader("EC measure");
      mst = stabOk() ? T_OK : T_WARN;
      snprintf(buf, sizeof(buf), "%.0f", R);
      calTile("R", buf, "ohm", mst, 6);
      calStabBar(244);
      calHint("short = capture   long = cancel");
      break;

    case CS_ORP_SEL:
      calHeader("ORP standard");
      calTile("ORP STD", ORP_STD_LBL[s_orpSel], "", T_WARN, 5);
      calHint("short = cycle   long = select");
      break;
    case CS_ORP_MEAS:
      calHeader("ORP measure");
      mst = stabOk() ? T_OK : T_WARN;
      snprintf(buf, sizeof(buf), "%.1f", ORPv);
      calTile("ORP", buf, "mV", mst, 6);
      calStabBar(244);
      calHint("short = capture   long = cancel");
      break;

    // Cyclops 2-point: blank (DI water) then the known standard (set its value in the portal)
    case CS_CYC_RB: calHeader("Cyclops blank"); calTile("PLACE IN", "DI", "blank/zero", T_OK, 5); calHint("short = read   long = back"); break;
    case CS_CYC_RS:
      calHeader("Cyclops std");
      snprintf(buf, sizeof(buf), "%.0f", deploy.cyc_std);
      calTile("STD", (deploy.cyc_std > 0 ? buf : "set!"), (deploy.cyc_std > 0 ? deploy.cyc_units : "in portal"), T_WARN, 5);
      calHint("short = read   long = back");
      break;
    case CS_CYC_MB: case CS_CYC_MS:
      calHeader(s_step == CS_CYC_MB ? "Cyclops blank" : "Cyclops std");
      if (!g_cycOk) { calTile("NO ADC", "--", "check ADS1015", T_ALARM, 4); calHint("long = cancel"); break; }
      mst = stabOk() ? T_OK : T_WARN;
      snprintf(buf, sizeof(buf), "%.3f", g_cycV);
      calTile(s_step == CS_CYC_MB ? "BLANK" : "STD", buf, "V", mst, 5);
      calStabBar(244);
      calHint("short = capture   long = cancel");
      break;

    case CS_SAVE: calHeader("saving");   calTile("SAVED", "OK", "rebooting", T_OK, 4); break;
    case CS_QUIT: calHeader("exit");     calTile("NO SAVE", "--", "rebooting", T_OK, 4); break;
  }
}
