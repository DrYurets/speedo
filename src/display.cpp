#include "display.h"
#include "pins.h"
#include <U8g2lib.h>

// SH1107 128x128 по I2C. Если картинка сдвинута/не выводится — попробовать
// U8G2_SH1107_PIMORONI_128X128_F_HW_I2C или U8G2_SH1107_SEEED_128X128_F_HW_I2C.
static U8G2_SH1107_128X128_F_HW_I2C u8g2(
    U8G2_R0, /* reset=*/ U8X8_PIN_NONE,
    /* clock=*/ PIN_OLED_SCL, /* data=*/ PIN_OLED_SDA);

static const char* const COMPASS[] = {"С", "СВ", "В", "ЮВ", "Ю", "ЮЗ", "З", "СЗ"};

const char* const MENU_ITEMS[] = {
    "Сброс поездки",
    "Сброс одометра",
    "Яркость",
    "Обновление (WiFi)",
    "Выход",
};
const uint8_t MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

void displayInit() {
  u8g2.begin();
  u8g2.setBusClock(400000);   // I2C 400 кГц
  u8g2.setContrast(255);
}

void displayContrast(uint8_t v) { u8g2.setContrast(v); }

static const char* compass8(float deg) {
  return COMPASS[(int)((deg + 22.5f) / 45.0f) % 8];
}

// Верхняя строка: спутники | курс | АКБ
static void drawTopBar(const Telemetry& t) {
  char buf[24];

  // иконка спутника (панель-корпус-панель)
  u8g2.drawBox(0, 3, 4, 4);
  u8g2.drawBox(5, 4, 4, 4);
  u8g2.drawBox(10, 3, 4, 4);

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "%d", t.sats);
  u8g2.drawStr(16, 10, buf);

  if (t.fixValid && t.speedKmh >= 3.0f) {
    const char* c = compass8(t.courseDeg);
    u8g2.drawUTF8(64 - u8g2.getUTF8Width(c) / 2, 10, c);
  }

  // батарея: рамка 16x8 + клемма + заливка
  int bx = 128 - 34, by = 2;
  u8g2.drawFrame(bx, by, 16, 8);
  u8g2.drawBox(bx + 16, by + 2, 2, 4);
  if (t.batCharging) {
    u8g2.drawBox(bx + 1, by + 1, 14, 6);
  } else {
    u8g2.drawBox(bx + 1, by + 1, 14 * t.batPct / 100, 6);
  }
  snprintf(buf, sizeof(buf), "%.1f", t.batV);
  u8g2.drawStr(bx + 20, 10, buf);

  u8g2.drawHLine(0, 13, 128);
}

void drawSpeedo(const Telemetry& t) {
  char buf[32];
  u8g2.clearBuffer();
  drawTopBar(t);

  u8g2.setFont(u8g2_font_logisoso32_tn);
  if (t.fixValid) {
    snprintf(buf, sizeof(buf), "%d", (int)(t.speedKmh + 0.5f));
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 60, buf);

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("км/ч") / 2, 78, "км/ч");

  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "Поездка  %.1f км", t.tripKm);
  u8g2.drawUTF8(4, 98, buf);
  snprintf(buf, sizeof(buf), "Всего  %.1f км", t.odoKm);
  u8g2.drawUTF8(4, 112, buf);
  snprintf(buf, sizeof(buf), "Макс     %d км/ч", (int)(t.maxSpeedKmh + 0.5f));
  u8g2.drawUTF8(4, 126, buf);

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

  snprintf(buf, sizeof(buf), "Курс      %s %d°",
           t.fixValid ? compass8(t.courseDeg) : "--",
           (int)(t.courseDeg + 0.5f));
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
  u8g2.drawUTF8(4, 56, buf);
  snprintf(buf, sizeof(buf), "HDOP      %.1f", t.hdop);
  u8g2.drawUTF8(4, 70, buf);
  snprintf(buf, sizeof(buf), "Широта   %.5f", t.lat);
  u8g2.drawUTF8(4, 84, buf);
  snprintf(buf, sizeof(buf), "Долгота  %.5f", t.lon);
  u8g2.drawUTF8(4, 98, buf);
  snprintf(buf, sizeof(buf), "Высота    %.0f м", t.altitudeM);
  u8g2.drawUTF8(4, 112, buf);
  snprintf(buf, sizeof(buf), "Данные    %u с назад", (unsigned)(t.gpsAgeMs / 1000));
  u8g2.drawUTF8(4, 126, buf);

  u8g2.sendBuffer();
}

void drawMenu(uint8_t cursor) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2.drawUTF8(64 - u8g2.getUTF8Width("МЕНЮ") / 2, 22, "МЕНЮ");
  u8g2.drawHLine(0, 28, 128);

  u8g2.setFont(u8g2_font_9x15_t_cyrillic);
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    int y = 46 + i * 17;
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

  if (t.otaProgress >= 0) {
    u8g2.drawFrame(14, 70, 100, 14);
    u8g2.drawBox(15, 71, 98 * t.otaProgress / 100, 12);
    snprintf(buf, sizeof(buf), "%d%%", t.otaProgress);
    u8g2.drawStr(64 - u8g2.getStrWidth(buf) / 2, 100, buf);
  } else {
    u8g2.drawUTF8(4, 92, "pio run -e esp32-c3-ota -t upload");
    u8g2.drawUTF8(4, 112, "BACK — выход");
  }
  u8g2.sendBuffer();
}
