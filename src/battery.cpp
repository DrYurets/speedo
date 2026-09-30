#include "battery.h"
#include "pins.h"

// Делитель 100k/22k: Vbat = Vadc * (122/22) = Vadc * 5.4545
// После монтажа откалибровать BAT_CAL по мультиметру на реальной АКБ.
static const float DIVIDER  = (100.0f + 22.0f) / 22.0f;
static const float BAT_CAL  = 1.0f;
static const float CHARGE_V = 13.2f;  // выше — работает генератор

void batteryInit() {
  pinMode(PIN_BAT_ADC, INPUT);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);  // шкала ~3.1 В
}

void batteryFill(Telemetry& t) {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) {
    mv += analogReadMilliVolts(PIN_BAT_ADC);
    delayMicroseconds(150);
  }
  mv /= 16;
  float v = mv / 1000.0f * DIVIDER * BAT_CAL;

  t.batV        = v;
  t.batCharging = v > CHARGE_V;

  // Карта свинцовой АКБ в покое: 11.8=0%, 12.0=25%, 12.2=50%, 12.4=75%, 12.6=100%
  static const float vx[] = {11.8f, 12.0f, 12.2f, 12.4f, 12.6f};
  static const int   px[] = {0, 25, 50, 75, 100};
  int pct = 0;
  if (v <= vx[0]) pct = 0;
  else if (v >= vx[4]) pct = 100;
  else {
    for (int i = 0; i < 4; i++) {
      if (v < vx[i + 1]) {
        pct = px[i] + (int)((v - vx[i]) * (px[i + 1] - px[i]) / (vx[i + 1] - vx[i]));
        break;
      }
    }
  }
  t.batPct = pct;
}
