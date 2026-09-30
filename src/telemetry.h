#pragma once
#include <Arduino.h>

// Единая структура телеметрии. Модули пишут сюда, UI отсюда рисует.
struct Telemetry {
  // GPS
  bool     fixValid    = false;
  float    speedKmh    = 0;
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

  // ТО (замена масла)
  double   svcOdoKm      = 0;  // одометр на момент последнего ТО
  uint16_t svcIntervalKm = 0;  // интервал ТО, км (5000–15000 шаг 500)

  // АКБ
  float    batV        = 0;
  int      batPct      = 0;
  bool     batCharging = false;

  // UI
  bool     needleMode  = false; // false = только цифры, true = стрелка+цифра
  bool     barScale    = true;  // линейная шкала под цифрами (режим «цифры»)

  // OTA
  bool     otaActive   = false;
  String   otaStatus;       // имя сети / "Заливка...", "Ошибка"
  String   otaIp;           // IP точки доступа (192.168.4.1)
  int      otaProgress = -1; // -1 = нет передачи, 0..100 = идёт заливка
};

extern Telemetry tele;
