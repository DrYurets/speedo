#pragma once
#include <Arduino.h>

// Единая структура телеметрии. Модули пишут сюда, UI отсюда рисует.
struct Telemetry {
  // GPS
  bool     fixValid    = false;
  float    speedKmh    = 0;
  float    courseDeg   = 0;
  uint8_t  sats        = 0;
  float    hdop        = 99.9f;
  double   lat         = 0;
  double   lon         = 0;
  float    altitudeM   = 0;
  uint32_t gpsAgeMs    = 0;

  // Одометр
  float    tripKm      = 0;
  double   odoKm       = 0;
  float    maxSpeedKmh = 0;
  uint32_t tripSec     = 0;

  // АКБ
  float    batV        = 0;
  int      batPct      = 0;
  bool     batCharging = false;

  // OTA
  bool     otaActive   = false;
  String   otaStatus;       // "Подключение...", IP, "Готово..."
  int      otaProgress = -1; // -1 = нет передачи, 0..100 = идёт заливка
};

extern Telemetry tele;
