#include "gps.h"
#include "pins.h"
#include <TinyGPSPlus.h>

static TinyGPSPlus gps;

void gpsInit() {
  // Serial1: RX=GPIO3 (от GPS TX), TX=GPIO2 (к GPS RX, опционально)
  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

void gpsUpdate() {
  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }
}

void gpsFill(Telemetry& t) {
  bool locOk  = gps.location.isValid() && gps.location.age() < 3000;
  bool hdopOk = !gps.hdop.isValid() || gps.hdop.hdop() < 5.0f;
  t.fixValid  = locOk && hdopOk;

  t.sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
  t.hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9f;
  t.gpsAgeMs = gps.location.isValid() ? gps.location.age() : 0;

  if (gps.location.isValid()) {
    t.lat = gps.location.lat();
    t.lon = gps.location.lng();
  }
  if (gps.altitude.isValid()) t.altitudeM = gps.altitude.meters();

  bool spdOk = gps.speed.isValid() && gps.speed.age() < 3000;
  t.speedKmh = spdOk ? gps.speed.kmph() : 0;
}
