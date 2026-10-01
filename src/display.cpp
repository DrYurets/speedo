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

static char svcLabel[20] = "ТО: 10000 км";

const char* MENU_ITEMS[] = {
    "Сброс поездки",
    "Одометр: задать",
    "Вид: цифры",
    "Шкала: вкл",
    "Часы: вкл",
    "HUD: выкл",
    svcLabel,
    "ТО пройдено",
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

void menuSetClockLabel(bool on) {
  MENU_ITEMS[4] = on ? "Часы: вкл" : "Часы: выкл";
}

void menuSetHudLabel(bool on) {
  MENU_ITEMS[5] = on ? "HUD: вкл" : "HUD: выкл";
}

void menuSetSvcLabel(uint16_t km) {
  snprintf(svcLabel, sizeof(svcLabel), "ТО: %u км", km);
}

void displayInit() {
  u8g2.begin();
  u8g2.setBusClock(400000);   // I2C 400 кГц
  u8g2.setContrast(255);
}

void displayContrast(uint8_t v) { u8g2.setContrast(v); }

// Зеркало для HUD (экран лицом вверх на торпеде, читаем в отражении
// лобового). Контроллеру шлём команды напрямую — u8g2.setFlipMode
// делает 180°, а нужна одна ось. Если ось не та — заменить пару
// на 0xc0/0xc8 (COM scan direction).
void displayMirror(bool on) {
  u8g2.sendF("c", on ? 0xa1 : 0xa0);   // SH1107 segment remap
}

// Верхняя строка: уровень сигнала (палочки) | одометр | иконка батареи
static void drawTopBar(const Telemetry& t) {
  // палочки как в телефоне: 1 палочка = 1 спутник, максимум 5;
  // больше 5 спутников — 5 палочек + маленький «+» после индикатора;
  // нет спутников → крестик
  if (t.sats == 0) {
    u8g2.drawLine(2, 2, 11, 11);
    u8g2.drawLine(11, 2, 2, 11);
    u8g2.drawLine(3, 2, 12, 11);
    u8g2.drawLine(12, 2, 3, 11);
  } else {
    uint8_t bars = t.sats > 5 ? 5 : t.sats;
    for (uint8_t i = 0; i < bars; i++) {
      int bh = 3 + i * 2;          // высоты 3,5,7,9,11
      u8g2.drawBox(i * 4, 11 - bh, 3, bh);
    }
    if (t.sats > 5) {
      u8g2.drawHLine(21, 7, 5);    // «+» после палочек
      u8g2.drawVLine(23, 5, 5);
    }
  }

  // одометр точно по центру верхней строки
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  char obuf[16];
  snprintf(obuf, sizeof(obuf), "%.0f", t.odoKm);
  int ow = u8g2.getUTF8Width(obuf);
  u8g2.drawUTF8(64 - ow / 2, 10, obuf);

  // иконка просрочки ТО: треугольник с «!» справа от одометра
  if (t.svcIntervalKm && t.odoKm - t.svcOdoKm > (double)t.svcIntervalKm) {
    int ix = 64 + ow / 2 + 5;
    u8g2.drawTriangle(ix + 5, 1, ix, 12, ix + 10, 12);
    u8g2.drawVLine(ix + 5, 4, 4);
    u8g2.drawPixel(ix + 5, 10);
  }

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
  // 0 км/ч → первый штрих (10), максимум → последний (110)
  float a  = speedAngle(10.0f + kmh * (110.0f - 10.0f) / SPD_MAX);
  int   nx = DIAL_CX + (int)roundf(cosf(a) * 48);
  int   ny = DIAL_CY + (int)roundf(sinf(a) * 48);
  // жирная стрелка — три линии
  u8g2.drawLine(DIAL_CX - 1, DIAL_CY, nx - 1, ny);
  u8g2.drawLine(DIAL_CX,     DIAL_CY, nx,     ny);
  u8g2.drawLine(DIAL_CX + 1, DIAL_CY, nx + 1, ny);
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

  // HUD: только скорость максимально крупно (картинка зеркалится
  // контроллером через displayMirror)
  if (t.hudMode) {
    if (t.fixValid) {
      int v = (int)(t.speedKmh + 0.5f);
      // 3 знака → поменьше, 1–2 знака → максимальный шрифт
      u8g2.setFont(v >= 100 ? u8g2_font_logisoso62_tn
                            : u8g2_font_logisoso92_tn);
      snprintf(buf, sizeof(buf), "%d", v);
      int y = 64 + (u8g2.getAscent() + u8g2.getDescent()) / 2;
      u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, y, buf);
    }
    u8g2.sendBuffer();
    return;
  }

  drawTopBar(t);

  if (t.needleMode) {
    drawDialTicks();
    drawNeedle(t.fixValid ? t.speedKmh : 0);   // стрелка всегда, без данных — на нуле
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

  // часы по центру нижней строки (UTC+3)
  if (t.clockShow && t.timeValid) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.clockH, t.clockM);
    u8g2.drawUTF8(64 - u8g2.getUTF8Width(buf) / 2, 124, buf);
  }

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

void drawService(const Telemetry& t) {
  char buf[40];
  u8g2.clearBuffer();
  drawTopBar(t);

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("ТО") / 2, 36, "ТО");

  double sinceSvc = t.odoKm - t.svcOdoKm;
  double left     = (double)t.svcIntervalKm - sinceSvc;

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "Интервал   %u км", (unsigned)t.svcIntervalKm);
  u8g2.drawUTF8(4, 60, buf);
  snprintf(buf, sizeof(buf), "Последнее  %.0f км", t.svcOdoKm);
  u8g2.drawUTF8(4, 76, buf);
  snprintf(buf, sizeof(buf), "Пройдено   %.0f км", sinceSvc);
  u8g2.drawUTF8(4, 92, buf);
  if (left >= 0) snprintf(buf, sizeof(buf), "Осталось   %.0f км", left);
  else           snprintf(buf, sizeof(buf), "ПРОСРОЧЕНО %.0f км", -left);
  u8g2.drawUTF8(4, 108, buf);

  u8g2.sendBuffer();
}

// Редактор одометра: 6 цифр, MODE — +1 к цифре, SET — следующая,
// после последней — сохранение, BACK — отмена.
void drawOdoEdit(const char* digits, uint8_t pos) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("ОДОМЕТР") / 2, 30, "ОДОМЕТР");

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  int dw = u8g2.getStrWidth("0");
  int x0 = 64 - dw * 3;
  for (uint8_t i = 0; i < 6; i++) {
    u8g2.drawGlyph(x0 + i * dw, 62, digits[i]);
  }
  u8g2.drawBox(x0 + pos * dw, 66, dw, 2);   // курсор под цифрой

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(4, 96, "MODE — цифра, SET — далее");
  u8g2.drawUTF8(4, 112, "BACK — отмена");
  u8g2.sendBuffer();
}

void drawMenu(uint8_t cursor) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("МЕНЮ") / 2, 17, "МЕНЮ");
  u8g2.drawHLine(0, 22, 128);

  // пунктов больше, чем влезает — прокрутка окном
  const uint8_t VIS = 7;
  uint8_t first = 0;
  if (cursor >= VIS) first = cursor - VIS + 1;

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  for (uint8_t i = first; i < MENU_COUNT && i < first + VIS; i++) {
    int y = 35 + (i - first) * 13;
    if (i == cursor) u8g2.drawUTF8(4, y, ">");
    u8g2.drawUTF8(18, y, MENU_ITEMS[i]);
  }
  // треугольники «есть ещё» сверху/снизу справа
  if (first > 0)
    u8g2.drawTriangle(118, 31, 124, 31, 121, 26);
  if (first + VIS < MENU_COUNT)
    u8g2.drawTriangle(118, 117, 124, 117, 121, 122);
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
