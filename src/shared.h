#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "MS5837.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>     // v7.x  (Library Manager: "ArduinoJson")
#include <Adafruit_ADS1X15.h> // Library Manager: "Adafruit ADS1X15"  (for the Qwiic ADS1015)

// ---------------- pins ----------------
#define PIN_BUTTON  D0   // pressure-safe twist actuator, momentary, active-low (provisional)
#define PIN_TFT_CS  D1
#define PIN_SD_CS   D2
#define PIN_TFT_DC  D3
#define PIN_SDA     D4
#define PIN_SCL     D5
#define PIN_TFT_RST D6
#define PIN_BL      D7
#define PIN_SCK     D8
#define PIN_MISO    D9
#define PIN_MOSI    D10

#define POET_ADDR   0x1F
#define BAR30_ADDR  0x76   // Blue Robotics BAR30 (MS5837-30BA) I2C address

// ---- Cyclops-7F fluorometer via SparkFun Qwiic ADS1015 (12-bit I2C ADC) ----
// The Cyclops outputs 0-5 V.  The ADS1015 on a 3.3 V Qwiic bus must NOT see >~3.6 V on an
// input, so a resistor divider is REQUIRED ahead of the ADC.  CYC_DIVIDER is the divider's
// multiplier: with 10k/10k (halves the voltage) it is 2.0, mapping 0-5 V -> 0-2.5 V at the pin.
#define ADS_ADDR     0x48      // ADS1015 default (ADDR -> GND)
#define CYC_ADC_CH   0         // ADS1015 single-ended channel the Cyclops "+" feeds (via divider)
#define CYC_DIVIDER  2.0f      // (R_top + R_bot)/R_bot ; 10k/10k = 2.0

// ---------------- tunables ----------------
#define SD_SPI_HZ          4000000UL
#define SAMPLE_MS          4000
#define POET_WAIT_MS       2788
#define BTN_DEBOUNCE_MS    25
#define BTN_LONG_MS        900
#define BTN_BOOTHOLD_MS    1500       // hold at power-on -> calibration mode
#define EC_SUBMERGED_NA    50L
#define SUBMERGE_DEBOUNCE  3
#define SURFACE_DEBOUNCE   3
#define POI_BANNER_MS      2000
#define C_STD_SEAWATER     42.914     // mS/cm, C(35,15,0) for PSS-78

// ---- screen backlight auto-dim (power saving) ----
// After dimMinutesEffective() of no twist activity the backlight FADES off over BL_FADE_MS.
// A twist wakes it (and that first wake-twist is swallowed -- no POI / no page flip).
#define BL_PWM_FREQ        5000U      // Hz -- above any visible flicker
#define BL_PWM_RES         8          // bits -> duty 0..255
#define BL_LEVEL_MAX       255        // full-brightness PWM duty
#define BL_FADE_MS         1500UL     // fade-to-off duration once the idle timeout elapses
#define DIM_MIN_MIN        1          // portal slider range (minutes)
#define DIM_MIN_MAX        240
#define DIM_MIN_DEFAULT    10         // default idle-to-dim timeout

// ---------------- firmware version ----------------
#define FW_VERSION         "0.7.0"

// display orientation: 0/2 = portrait (240x320), 1/3 = landscape (320x240).
// Unit is held vertically -> portrait.  Flip 2<->0 if the image is upside-down.
#define TFT_ROT            2
#define SCR_W              240
#define SCR_H              320

// ---------------- types ----------------
struct PoetResult { int32_t temp_mC, orp_uV, ugs_uV, ec_nA, ec_uV; };

enum NavEvent { NAV_NONE, NAV_SHORT, NAV_LONG };
enum AppMode  { MODE_RUN, MODE_CAL };

struct CalData {
  bool  ph_valid;  float ph_ugs7_mV, ph_slope, ph_calT;   // pH = 7 + (Ugs-ugs7)/slope
  bool  ec_valid;  float ec_K, ec_calT;                   // K = R * kappa_std (1/cm)
  bool  orp_valid; float orp_offset_mV;                   // Eh = orp_mV + offset
  bool  cyc_valid; float cyc_v0, cyc_k;                   // conc = k*(V - v0)  (2-pt direct conc.)
  uint32_t ph_epoch, ec_epoch, orp_epoch, cyc_epoch;
};

// ---- screen-2 metric thresholds (set per-mission in the portal; NAN = bound disabled) ----
enum MetricId { M_TEMP, M_PH, M_ORP, M_EC, M_SAL, M_DEPTH, M_CYC, M_COUNT };
struct Threshold { float warnLo, warnHi, alarmLo, alarmHi; };

struct Deployment {
  char     mission[32];
  char     op[24];
  char     site[48];
  char     waterType[12];
  char     weather[24];
  char     notes[64];
  double   lat, lon;  bool hasPos;
  float    airTemp;   bool hasWeather;
  uint32_t castNum;
  bool     set;
  uint8_t  accent;             // UI base colour: 0 teal, 1 orange, 2 green, 3 red
  bool     cyc_en;             // Cyclops fluorometer fitted/enabled
  char     cyc_units[8];       // display/log units label, e.g. "ug/L", "NTU", "ppb"
  float    cyc_std;            // known concentration of the calibration standard
  uint16_t dimMinutes;         // backlight idle-to-dim timeout (min); 0/out-of-range -> DIM_MIN_DEFAULT
  Threshold thresh[M_COUNT];   // alarm bands rendered on the metrics page
};

// ---- shared screen styling (palette + tile primitive used by every page) ----
#define COL_BORDER 0x4208   // dim grey  (~RGB 64,64,64)
#define COL_LABEL  0x8410   // grey      (~RGB 128,128,128)
#define COL_WARN   0xFD20   // amber  (warning band -- fixed, not themed)
enum TileState { T_OK, T_WARN, T_ALARM };

// ---------------- globals (defined in main .ino) ----------------
extern Adafruit_ST7789 tft;
extern MS5837    bar30;
extern PoetResult g_poet;
extern bool      g_poetOk, g_submerged, g_sdReady, g_logging;
extern float     g_P, g_depth, g_barT, g_ascent;
extern uint32_t  g_sampleCount, g_poiCount;
extern uint8_t   g_page;
extern AppMode   g_mode;
extern volatile NavEvent g_nav;
extern CalData   cal;
extern Deployment deploy;
extern uint16_t  g_accent;        // resolved RGB565 of deploy.accent (the OK / base colour)
extern volatile bool g_reqCal;    // portal -> request calibration-mode entry
extern volatile bool g_uiDirty;   // request a run-screen redraw (e.g. after a settings change)
extern bool      g_cycOk;         // ADS1015 present
extern float     g_cycV;          // latest Cyclops output voltage (0-5 V, after divider correction)

// time (best-effort until RTC revision)
extern bool      g_timeSynced, g_timeApprox;
extern uint32_t  g_epoch0, g_epochAtMillis;

// ---------------- prototypes ----------------
// main
bool sdEnsure();
uint32_t nowUnix();
void isoTime(uint32_t t, char *out, size_t n);
void renderRun();
void runHandleNav(NavEvent e);
uint16_t accentColor(uint8_t idx);

// backlight auto-dim (defined in main .ino)
void     backlightBegin();           // attach PWM, start at full brightness
void     backlightLoop();            // call every loop(): runs the idle->fade->off timer
bool     backlightWake();            // restore full brightness + reset timer; true if it WAS dimmed
uint16_t dimMinutesEffective();      // clamped idle timeout (DIM_MIN_DEFAULT if unset/out of range)

// shared UI primitive (defined in main .ino, used by run + cal screens)
TileState tileState(MetricId m, float v);
void drawTile(int16_t x, int16_t y, int16_t w, int16_t h,
              const char *label, const char *val, const char *units,
              TileState st, uint8_t maxVs = 4);

// poet
bool poetStart();
bool poetFetch(PoetResult &out);

// calibration module
void calLoad();
void calBegin();
void calOnSample();
void calHandleNav(NavEvent e);
void calRender();
float phFromUgs(int32_t ugs_uV, float T);
float conductivity_mS(int32_t ec_uV, int32_t ec_nA);
float specCond25_mS(float c_mS, float T);
float orpEh_mV(int32_t orp_uV);
float salinityPSU(float c_mS, float T, float P_mbar);
float cycConc(float volts);

// setup portal (WiFi UTC sync + deployment header + thresholds + accent)
void portalBegin();
void portalLoop();
void portalEnd();
bool portalActive();
void setUnixFromMillisEpoch(uint64_t epoch_ms);
void stateSave();
void stateLoad();

#endif
