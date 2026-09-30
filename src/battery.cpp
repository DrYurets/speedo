#include "battery.h"
#include "pins.h"

// Делитель 1:3 — верхнее плечо 100 кОм, нижнее 100к||100к = 50 кОм:
// Vmeas = Vadc * 3. Верхняя граница ~9 В, вход до 5 В занимает ~55% шкалы.
// ADC ESP32-C3, ADC_11db (=12 дБ): диапазон ~0–3.1 В (datasheet).
// После монтажа откалибровать BAT_CAL по мультиметру.
static const float DIVIDER  = 3.0f;
static const float BAT_CAL  = 1.0f;
static const float CHARGE_V = 4.25f;  // выше 4.2 В — подключена зарядка/5 В

void batteryInit() {
  pinMode(PIN_BAT_ADC, INPUT);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);  // 12 дБ, ~0–3.1 В
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

  // Карта Li-ion 18650 (примерная кривая разряда):
  // 3.2=0%, 3.5=10%, 3.6=25%, 3.8=50%, 4.0=75%, 4.2=100%
  static const float vx[] = {3.2f, 3.5f, 3.6f, 3.8f, 4.0f, 4.2f};
  static const int   px[] = {0, 10, 25, 50, 75, 100};
  const int n = sizeof(vx) / sizeof(vx[0]);
  int pct;
  if (v <= vx[0]) pct = 0;
  else if (v >= vx[n - 1]) pct = 100;
  else {
    pct = 0;
    for (int i = 0; i < n - 1; i++) {
      if (v < vx[i + 1]) {
        pct = px[i] + (int)((v - vx[i]) * (px[i + 1] - px[i]) / (vx[i + 1] - vx[i]));
        break;
      }
    }
  }
  t.batPct = pct;
}
