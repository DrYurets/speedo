#include "display.h"
#include "pins.h"
#include <U8g2lib.h>

// SH1107 128x128 по I2C. Если картинка сдвинута/не выводится — попробовать
// U8G2_SH1107_PIMORONI_128X128_F_HW_I2C или U8G2_SH1107_SEEED_128X128_F_HW_I2C.
// У этого модуля (4-pin I2C) панель повернута относительно контроллера —
// нужен поворот (U8G2_R1 = 90°; если вверх ногами — U8G2_R3).
// Если картинка сдвинута/заворачивается — сменить конструктор:
// U8G2_SH1107_128X128_F_HW_I2C, U8G2_SH1107_SEEED_128X128_F_HW_I2C.
static U8G2_SH1107_PIMORONI_128X128_F_HW_I2C u8g2(
    U8G2_R1, /* reset=*/ U8X8_PIN_NONE,
    /* clock=*/ PIN_OLED_SCL, /* data=*/ PIN_OLED_SDA);

const char* MENU_ITEMS[] = {
    "Сброс поездки",
    "Сброс одометра",
    "Вид: цифры",
    "Шкала: вкл",
    "Яркость",
    "Обновление (WiFi)",
    "Выход",
};
const uint8_t MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

void menuSetDialLabel(bool needle) {
  MENU_ITEMS[2] = needle ? "Вид: стрелка" : "Вид: цифры";
}

void menuSetBarLabel(bool on) {
  MENU_ITEMS[3] = on ? "Шкала: вкл" : "Шкала: выкл";
}

void displayInit() {
  u8g2.begin();
  u8g2.setBusClock(400000);   // I2C 400 кГц
  u8g2.setContrast(255);
}

void displayContrast(uint8_t v) { u8g2.setContrast(v); }

// Верхняя строка: уровень сигнала (палочки) | одометр | иконка батареи
static void drawTopBar(const Telemetry& t) {
  // палочки как в телефоне: максимум 5, высота нарастает;
  // незначащие палочки не рисуем; нет спутников → крестик
  // 1-3 → 1; 4-6 → 2; 7-9 → 3; 10-12 → 4; 13+ → 5
  if (t.sats == 0) {
    u8g2.drawLine(2, 2, 11, 11);
    u8g2.drawLine(11, 2, 2, 11);
    u8g2.drawLine(3, 2, 12, 11);
    u8g2.drawLine(12, 2, 3, 11);
  } else {
    uint8_t bars = (uint8_t)((t.sats + 2) / 3);
    if (bars > 5) bars = 5;
    for (uint8_t i = 0; i < bars; i++) {
      int bh = 3 + i * 2;          // высоты 3,5,7,9,11
      u8g2.drawBox(i * 4, 11 - bh, 3, bh);
    }
  }

  // одометр точно по центру верхней строки
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  char obuf[16];
  snprintf(obuf, sizeof(obuf), "%.0f", t.odoKm);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width(obuf) / 2, 10, obuf);

  // батарея в правом углу: рамка 16x8 + клемма + заливка
  int bx = 128 - 18, by = 2;
  u8g2.drawFrame(bx, by, 16, 8);
  u8g2.drawBox(bx + 16, by + 2, 2, 4);
  if (t.batCharging) {
    u8g2.drawBox(bx + 1, by + 1, 14, 6);
  } else {
    u8g2.drawBox(bx + 1, by + 1, 14 * t.batPct / 100, 6);
  }
}

// Шкала спидометра: пунктирная дуга ~270°, деления по 10 км/ч до 120 км/ч,
// без крайних рисок (0 и 120); риски на 60 и 90 длиннее и толще.
// Центр дуги (64,74), радиус 57.
static const int   DIAL_CX = 64, DIAL_CY = 74, DIAL_R = 57;
static const float SPD_MAX = 120.0f;

// 0 км/ч → 135° (лево-низ), 120 км/ч → 45° (право-низ); ход по часовой
static float speedAngle(float kmh) {
  return (135.0f + 270.0f * kmh / SPD_MAX) * (float)M_PI / 180.0f;
}

static void drawDialTicks() {
  for (int s = 10; s < 120; s += 10) {
    float a   = speedAngle(s);
    bool  big = (s == 60 || s == 90);
    int   rIn = big ? 45 : 51;
    int   x1  = DIAL_CX + (int)roundf(cosf(a) * rIn);
    int   y1  = DIAL_CY + (int)roundf(sinf(a) * rIn);
    int   x2  = DIAL_CX + (int)roundf(cosf(a) * DIAL_R);
    int   y2  = DIAL_CY + (int)roundf(sinf(a) * DIAL_R);
    u8g2.drawLine(x1, y1, x2, y2);
    if (big) {  // вторая линия со смещением по углу — риска толще
      float a2 = a + 0.04f;
      u8g2.drawLine(DIAL_CX + (int)roundf(cosf(a2) * rIn),
                    DIAL_CY + (int)roundf(sinf(a2) * rIn),
                    DIAL_CX + (int)roundf(cosf(a2) * DIAL_R),
                    DIAL_CY + (int)roundf(sinf(a2) * DIAL_R));
    }
  }
}

static void drawNeedle(float kmh) {
  if (kmh > SPD_MAX) kmh = SPD_MAX;
  if (kmh < 0) kmh = 0;
  float a  = speedAngle(kmh);
  int   nx = DIAL_CX + (int)roundf(cosf(a) * 48);
  int   ny = DIAL_CY + (int)roundf(sinf(a) * 48);
  // жирная стрелка — три линии
  u8g2.drawLine(DIAL_CX - 1, DIAL_CY, nx - 1, ny);
  u8g2.drawLine(DIAL_CX,     DIAL_CY, nx,     ny);
  u8g2.drawLine(DIAL_CX + 1, DIAL_CY, nx + 1, ny);
  // кружок на стрелке (~2/3 длины от оси)
  int kx = DIAL_CX + (int)roundf(cosf(a) * 31);
  int ky = DIAL_CY + (int)roundf(sinf(a) * 31);
  u8g2.drawCircle(kx, ky, 4);
  u8g2.drawDisc(DIAL_CX, DIAL_CY, 4);                 // ось
}

// Линейная шкала под цифрами: вертикальные штрихи 0..120 км/ч с шагом 10,
// 60 и 90 выше и толще. Над шкалой — треугольник-указатель скорости.
static void drawSpeedBar(float kmh, bool hasFix) {
  const int x0 = 6, x1 = 122, yBase = 111;
  for (int s = 0; s <= 120; s += 10) {
    int  x   = x0 + (x1 - x0) * s / 120;
    bool big = (s == 60 || s == 90);
    int  h   = big ? 16 : 10;
    u8g2.drawVLine(x, yBase - h + 1, h);
    if (big) u8g2.drawVLine(x + 1, yBase - h + 1, h);
  }
  if (hasFix) {
    if (kmh > 120.0f) kmh = 120.0f;
    if (kmh < 0) kmh = 0;
    int tx = x0 + (int)roundf((x1 - x0) * kmh / 120.0f);
    u8g2.drawTriangle(tx, 98, tx - 5, 91, tx + 5, 91);  // вершиной вниз
  }
}

void drawSpeedo(const Telemetry& t) {
  char buf[32];
  u8g2.clearBuffer();
  drawTopBar(t);

  if (t.needleMode) {
    drawDialTicks();
    if (t.fixValid) drawNeedle(t.speedKmh);
    u8g2.setFont(u8g2_font_logisoso32_tn);
    if (t.fixValid) {
      snprintf(buf, sizeof(buf), "%d", (int)(t.speedKmh + 0.5f));
      u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 110, buf);
    }
  } else {
    u8g2.setFont(u8g2_font_logisoso62_tn);
    if (t.fixValid) {
      snprintf(buf, sizeof(buf), "%d", (int)(t.speedKmh + 0.5f));
      u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 86, buf);
    }
    if (t.barScale) drawSpeedBar(t.speedKmh, t.fixValid);
  }

  // нижняя строка: поездка слева | макс справа
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  if (t.tripKm < 100.0f) snprintf(buf, sizeof(buf), "%.1f км", t.tripKm);
  else                   snprintf(buf, sizeof(buf), "%.0f км", t.tripKm);
  u8g2.drawUTF8(2, 124, buf);

  snprintf(buf, sizeof(buf), "%d км/ч", (int)(t.maxSpeedKmh + 0.5f));
  u8g2.drawUTF8(126 - u8g2.getUTF8Width(buf), 124, buf);

  u8g2.sendBuffer();
}

void drawTrip(const Telemetry& t) {
  char buf[40];
  u8g2.clearBuffer();
  drawTopBar(t);

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("ПОЕЗДКА") / 2, 36, "ПОЕЗДКА");

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  uint32_t s = t.tripSec;
  snprintf(buf, sizeof(buf), "Время     %u:%02u:%02u",
           s / 3600, (s / 60) % 60, s % 60);
  u8g2.drawUTF8(4, 58, buf);

  snprintf(buf, sizeof(buf), "Путь      %.2f км", t.tripKm);
  u8g2.drawUTF8(4, 74, buf);

  if (s >= 60) {
    snprintf(buf, sizeof(buf), "Средняя   %.1f км/ч", t.tripKm / (s / 3600.0f));
  } else {
    snprintf(buf, sizeof(buf), "Средняя   --");
  }
  u8g2.drawUTF8(4, 90, buf);

  snprintf(buf, sizeof(buf), "Максимум  %d км/ч", (int)(t.maxSpeedKmh + 0.5f));
  u8g2.drawUTF8(4, 106, buf);

  snprintf(buf, sizeof(buf), "Батарея   %.2f В", t.batV);
  u8g2.drawUTF8(4, 122, buf);

  u8g2.sendBuffer();
}

void drawGps(const Telemetry& t) {
  char buf[40];
  u8g2.clearBuffer();
  drawTopBar(t);

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("GPS") / 2, 36, "GPS");

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "Спутники  %d", t.sats);
  u8g2.drawUTF8(4, 52, buf);
  snprintf(buf, sizeof(buf), "HDOP      %.1f", t.hdop);
  u8g2.drawUTF8(4, 64, buf);
  snprintf(buf, sizeof(buf), "Широта   %.5f", t.lat);
  u8g2.drawUTF8(4, 76, buf);
  snprintf(buf, sizeof(buf), "Долгота  %.5f", t.lon);
  u8g2.drawUTF8(4, 88, buf);
  snprintf(buf, sizeof(buf), "Высота    %.0f м", t.altitudeM);
  u8g2.drawUTF8(4, 100, buf);
  snprintf(buf, sizeof(buf), "Данные    %u с назад", (unsigned)(t.gpsAgeMs / 1000));
  u8g2.drawUTF8(4, 112, buf);
  snprintf(buf, sizeof(buf), "Батарея   %.2f В", t.batV);
  u8g2.drawUTF8(4, 124, buf);

  u8g2.sendBuffer();
}

void drawMenu(uint8_t cursor) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("МЕНЮ") / 2, 20, "МЕНЮ");
  u8g2.drawHLine(0, 26, 128);

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    int y = 38 + i * 13;
    if (i == cursor) u8g2.drawUTF8(4, y, ">");
    u8g2.drawUTF8(18, y, MENU_ITEMS[i]);
  }
  u8g2.sendBuffer();
}

void drawConfirm(const char* question) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width(question) / 2, 56, question);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("SET — да, BACK — нет") / 2, 78,
                "SET — да, BACK — нет");
  u8g2.sendBuffer();
}

void drawOta(const Telemetry& t) {
  char buf[40];
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("ОБНОВЛЕНИЕ") / 2, 30, "ОБНОВЛЕНИЕ");

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(4, 56, t.otaStatus.c_str());
  if (t.otaIp.length()) {
    snprintf(buf, sizeof(buf), "IP: %s", t.otaIp.c_str());
    u8g2.drawUTF8(4, 70, buf);
  }

  if (t.otaProgress >= 0) {
    u8g2.drawFrame(14, 70, 100, 14);
    u8g2.drawBox(15, 71, 98 * t.otaProgress / 100, 12);
    snprintf(buf, sizeof(buf), "%d%%", t.otaProgress);
    u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 100, buf);
  } else {
    u8g2.drawUTF8(4, 96, "BACK — выход");
  }
  u8g2.sendBuffer();
}
