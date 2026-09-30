#include "odometer.h"
#include <Preferences.h>

// Порог скорости против GPS-шума на стоянке
static const float MIN_SPEED_KMH = 3.0f;
// Писать в NVS: каждые +0.5 км или каждые 60 с, если есть несохранённое
static const double SAVE_STEP_KM  = 0.5;
static const uint32_t SAVE_MS     = 60000;

static Preferences prefs;
static double   odoKm     = 0;
static float    tripKm    = 0;
static float    maxSpeed  = 0;
static uint32_t tripMs    = 0;
static uint32_t lastMs    = 0;
static uint32_t lastSaveMs = 0;
static double   savedKm   = 0;
static double   svcOdoKm  = 0;        // одометр на момент последнего ТО
static uint16_t svcIntKm  = 10000;    // интервал ТО по умолчанию

void odoInit() {
  prefs.begin("speedo", false);
  odoKm    = prefs.getDouble("odo_km", 0.0);
  svcOdoKm = prefs.getDouble("svc_odo", 0.0);
  svcIntKm = prefs.getUShort("svc_int", 10000);
  savedKm  = odoKm;
  lastMs   = millis();
}

void odoUpdate(const Telemetry& t) {
  uint32_t now = millis();
  uint32_t dt  = now - lastMs;
  lastMs = now;
  if (dt > 2000) dt = 0;  // сторож от скачков при блокировке loop (OTA и т.п.)

  if (t.fixValid) {
    tripMs += dt;
    if (t.speedKmh >= MIN_SPEED_KMH) {
      double dKm = (double)t.speedKmh / 3.6 * dt / 1000.0 / 1000.0;
      tripKm += (float)dKm;
      odoKm  += dKm;
    }
    if (t.speedKmh > maxSpeed) maxSpeed = t.speedKmh;
  }

  if (odoKm - savedKm >= SAVE_STEP_KM ||
      (odoKm != savedKm && now - lastSaveMs >= SAVE_MS)) {
    savedKm   = odoKm;
    lastSaveMs = now;
    prefs.putDouble("odo_km", odoKm);
  }
}

void odoFill(Telemetry& t) {
  t.tripKm      = tripKm;
  t.odoKm       = odoKm;
  t.maxSpeedKmh = maxSpeed;
  t.tripSec     = tripMs / 1000;
  t.svcOdoKm      = svcOdoKm;
  t.svcIntervalKm = svcIntKm;
}

void odoResetTrip() {
  tripKm = 0; maxSpeed = 0; tripMs = 0;
}

void odoResetTotal() {
  odoKm = 0; savedKm = 0;
  prefs.putDouble("odo_km", 0.0);
}

void odoSetTotal(double km) {
  odoKm = km; savedKm = km;
  prefs.putDouble("odo_km", km);
}

void svcSetInterval(uint16_t km) {
  svcIntKm = km;
  prefs.putUShort("svc_int", km);
}

uint16_t svcGetInterval() { return svcIntKm; }

void svcMarkDone() {
  svcOdoKm = odoKm;
  prefs.putDouble("svc_odo", svcOdoKm);
}
