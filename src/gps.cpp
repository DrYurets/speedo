#include "gps.h"
#include "pins.h"
#include <TinyGPSPlus.h>

static TinyGPSPlus gps;

// --- SNR из GSV-предложений ($GxGSV, TinyGPSPlus их не разбирает) ---
// Поля: talker(0), total(1), num(2), inView(3), далее группы по 4:
// svid(4), elev(5), azim(6), snr(7), ... SNR — поля 7,11,15,19.
static uint32_t snrSum = 0;
static uint8_t  snrCnt = 0;
static uint8_t  snrAvg = 0;

static void sniffNmea(const char* s) {
  if (s[0] != '$' || s[3] != 'G' || s[4] != 'S' || s[5] != 'V') return;
  int field = 0, num = 0, total = 0;
  for (const char* p = s; *p && *p != '*'; p++) {
    if (*p != ',') continue;
    field++;
    const char* q = p + 1;
    if (field == 1)      total = atoi(q);
    else if (field == 2) { num = atoi(q); if (num == 1) { snrSum = 0; snrCnt = 0; } }
    else if (field >= 7 && field % 4 == 3 && *q >= '0' && *q <= '9') {
      snrSum += atoi(q); snrCnt++;
    }
  }
  if (num && num == total && snrCnt) snrAvg = snrSum / snrCnt;
}

// Высота солнца над горизонтом, градусы (аппроксимация NOAA).
// <0 — солнце за горизонтом. Нужны lat/lon и UTC дата+время.
static float sunElevDeg(float lat, float lon, int year, int month, int day,
                        int hour, int minute) {
  static const int md[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int doy = md[month - 1] + day;
  if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) doy++;
  float g = 2.0f * M_PI / 365.0f * (doy - 1 + (hour - 12) / 24.0f);
  float decl = 0.006918f - 0.399912f*cosf(g) + 0.070257f*sinf(g)
             - 0.006758f*cosf(2*g) + 0.000907f*sinf(2*g)
             - 0.002697f*cosf(3*g) + 0.00148f*sinf(3*g);
  float eqt  = 229.18f * (0.000075f + 0.001868f*cosf(g) - 0.032077f*sinf(g)
             - 0.014615f*cosf(2*g) - 0.040849f*sinf(2*g));
  float ha   = ((hour * 60 + minute + eqt + 4 * lon) / 4.0f - 180.0f)
               * M_PI / 180.0f;   // часовой угол
  float phi  = lat * M_PI / 180.0f;
  float el   = asinf(sinf(phi)*sinf(decl) + cosf(phi)*cosf(decl)*cosf(ha));
  return el * 180.0f / M_PI;
}

void gpsInit() {
  // Serial1: RX=GPIO3 (от GPS TX), TX=GPIO2 (к GPS RX, опционально)
  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

void gpsUpdate() {
  static char line[96];
  static uint8_t ll = 0;
  while (Serial1.available()) {
    char ch = (char)Serial1.read();
    gps.encode(ch);
    if (ch == '$') ll = 0;
    if (ll < sizeof(line) - 1) line[ll++] = ch;
    if (ch == '\n') { line[ll] = 0; sniffNmea(line); }
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

  // Часы: UTC из GPS + фиксированный пояс UTC+3 (Минск, DST отменён)
  bool tOk = gps.time.isValid() && gps.time.age() < 3000;
  t.timeValid = tOk;
  if (tOk) {
    t.clockH = (gps.time.hour() + 3) % 24;
    t.clockM = gps.time.minute();
  }

  // Уровень сигнала для палочек: средний SNR; если GSV не приходят —
  // fallback на число спутников
  t.snrAvg = snrAvg;
  if (t.sats == 0)       t.sigBars = 0;
  else if (snrAvg >= 35) t.sigBars = 5;
  else if (snrAvg >= 30) t.sigBars = 4;
  else if (snrAvg >= 25) t.sigBars = 3;
  else if (snrAvg >= 18) t.sigBars = 2;
  else if (snrAvg > 0)   t.sigBars = 1;
  else                   t.sigBars = t.sats > 5 ? 5 : t.sats;

  // День/ночь по высоте солнца (для автояркости и авто-HUD)
  if (gps.date.isValid() && gps.time.isValid() && gps.location.isValid()
      && gps.time.age() < 3000) {
    t.isNight = sunElevDeg(t.lat, t.lon, gps.date.year(), gps.date.month(),
                           gps.date.day(), gps.time.hour(), gps.time.minute())
                < -2.0f;
  }
}
