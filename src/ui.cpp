#include "ui.h"
#include "pins.h"
#include "telemetry.h"
#include "display.h"
#include "odometer.h"
#include "ota.h"
#include <Preferences.h>

static const uint32_t DEBOUNCE_MS   = 30;
static const uint32_t LONG_MS       = 800;
static const uint32_t REDRAW_MS     = 200;
static const uint32_t OTA_TIMEOUT_MS = 300000;  // 5 мин

struct Button {
  uint8_t  pin;
  bool     stableHigh = true;
  bool     lastRaw    = true;
  uint32_t changedAt  = 0;
  uint32_t pressedAt  = 0;
  bool     longFired  = false;
};

enum class BtnEvent : uint8_t { NONE, SHORT_MODE, SHORT_SET, SHORT_BACK,
                                LONG_MODE, LONG_SET, LONG_BACK };

static Button bMode, bSet, bBack;

enum class Mode : uint8_t { SCREENS, MENU, CONFIRM, OTA, ODO_EDIT };
static Mode    mode       = Mode::SCREENS;
static uint8_t screenIdx  = 0;   // 0=Speedo 1=Trip 2=Service 3=Gps
static uint8_t menuCursor = 0;
static uint8_t confirmAct = 0;   // 0 нет, 1 сброс поездки, 3 ТО пройдено
static char    odoDigits[7];     // редактор одометра: 6 цифр + \0
static uint8_t odoPos     = 0;
static uint8_t brightIdx  = 0;
static uint32_t lastDraw  = 0;
static uint32_t otaStartMs = 0;

// 10 уровней яркости: индекс 0 = самый яркий (уровень 10), 9 = 1
static const uint8_t CONTRASTS[] = {255, 224, 192, 160, 128, 96, 64, 40, 24, 8};
static const uint8_t CONTRAST_COUNT = sizeof(CONTRASTS);

// Отработать кнопку; вернуть true при коротком нажатии, long через флаг
static bool pollBtn(Button& b, bool& longPress) {
  bool raw = digitalRead(b.pin);
  if (raw != b.lastRaw) { b.lastRaw = raw; b.changedAt = millis(); }
  if (raw == b.stableHigh) return false;
  if (millis() - b.changedAt < DEBOUNCE_MS) return false;

  b.stableHigh = raw;
  if (!raw) {  // нажата (LOW)
    b.pressedAt = millis();
    b.longFired = false;
  } else {     // отпущена
    if (!b.longFired && millis() - b.pressedAt < LONG_MS) return true;
  }
  return false;
}

static BtnEvent pollButtons() {
  bool lg = false;
  if (pollBtn(bMode, lg)) return BtnEvent::SHORT_MODE;
  if (pollBtn(bSet, lg))  return BtnEvent::SHORT_SET;
  if (pollBtn(bBack, lg)) return BtnEvent::SHORT_BACK;
  // длинные нажатия — проверка удержания
  if (!bMode.stableHigh && !bMode.longFired && millis() - bMode.pressedAt >= LONG_MS) {
    bMode.longFired = true;
    return BtnEvent::LONG_MODE;
  }
  if (!bSet.stableHigh && !bSet.longFired && millis() - bSet.pressedAt >= LONG_MS) {
    bSet.longFired = true;
    return BtnEvent::LONG_SET;
  }
  if (!bBack.stableHigh && !bBack.longFired && millis() - bBack.pressedAt >= LONG_MS) {
    bBack.longFired = true;
    return BtnEvent::LONG_BACK;
  }
  return BtnEvent::NONE;
}

static void render() {
  lastDraw = millis();
  displayMirror(tele.hudMode);   // в HUD весь вывод зеркальный
  switch (mode) {
    case Mode::SCREENS:
      if (screenIdx == 0)      drawSpeedo(tele);
      else if (screenIdx == 1) drawTrip(tele);
      else if (screenIdx == 2) drawService(tele);
      else                     drawGps(tele);
      break;
    case Mode::MENU:
      drawMenu(menuCursor);
      break;
    case Mode::CONFIRM:
      drawConfirm(confirmAct == 1 ? "Сбросить поездку?" : "Отметить ТО?");
      break;
    case Mode::ODO_EDIT:
      drawOdoEdit(odoDigits, odoPos);
      break;
    case Mode::OTA:
      drawOta(tele);
      break;
  }
}

static void odoEditEnter() {
  double v = tele.odoKm;
  if (v > 999999.0) v = 999999.0;
  if (v < 0) v = 0;
  snprintf(odoDigits, sizeof(odoDigits), "%06.0f", v);
  odoPos = 0;
}

static void hudSet(bool on) {
  if (tele.hudMode == on) return;
  tele.hudMode = on;
  menuSetHudLabel(on);
  Preferences p;
  p.begin("speedo", false);
  p.putBool("hud", on);
  p.end();
}

static void hudToggle() { hudSet(!tele.hudMode); }

// Долгое MODE: цикл цифры → цифры+шкала → стрелка
static void viewCycle() {
  if (tele.needleMode)   { tele.needleMode = false; tele.barScale = false; }
  else if (tele.barScale) tele.needleMode = true;
  else                    tele.barScale = true;
  menuSetDialLabel(tele.needleMode);
  menuSetBarLabel(tele.barScale);
  Preferences p;
  p.begin("speedo", false);
  p.putBool("needle", tele.needleMode);
  p.putBool("bar", tele.barScale);
  p.end();
}

// NVS-переключатель булевой настройки
static void prefToggle(const char* key, bool& flag) {
  flag = !flag;
  Preferences p;
  p.begin("speedo", false);
  p.putBool(key, flag);
  p.end();
}

static void menuSelect() {
  switch (menuCursor) {
    case 0: confirmAct = 1; mode = Mode::CONFIRM; break;
    case 1: odoEditEnter(); mode = Mode::ODO_EDIT; break;
    case 2:  // вид спидометра: цифры ⇄ стрелка+цифра
      tele.needleMode = !tele.needleMode;
      menuSetDialLabel(tele.needleMode);
      {
        Preferences p;
        p.begin("speedo", false);
        p.putBool("needle", tele.needleMode);
        p.end();
      }
      break;
    case 3:  // линейная шкала под цифрами: вкл ⇄ выкл
      tele.barScale = !tele.barScale;
      menuSetBarLabel(tele.barScale);
      {
        Preferences p;
        p.begin("speedo", false);
        p.putBool("bar", tele.barScale);
        p.end();
      }
      break;
    case 4:  // часы на главном экране: вкл ⇄ выкл
      tele.clockShow = !tele.clockShow;
      menuSetClockLabel(tele.clockShow);
      {
        Preferences p;
        p.begin("speedo", false);
        p.putBool("clock", tele.clockShow);
        p.end();
      }
      break;
    case 5:  // HUD: зеркальные крупные цифры скорости
      hudToggle();
      break;
    case 6:  // авто-HUD ночью
      prefToggle("ahud", tele.autoHud);
      menuSetAutoHudLabel(tele.autoHud);
      break;
    case 7: {  // интервал ТО: шаг 500, от 5000 до 15000, по кругу
      uint16_t next = svcGetInterval() + 500;
      if (next > 15000 || next < 5000) next = 5000;
      svcSetInterval(next);
      menuSetSvcLabel(next);
      break;
    }
    case 8: confirmAct = 3; mode = Mode::CONFIRM; break;
    case 9:
      brightIdx = (brightIdx + 1) % CONTRAST_COUNT;
      displayContrast(CONTRASTS[brightIdx]);
      menuSetBrightLabel(CONTRAST_COUNT - brightIdx);
      {
        Preferences p;
        p.begin("speedo", false);
        p.putUChar("bright", brightIdx);
        p.end();
      }
      break;
    case 10:  // автояркость день/ночь
      prefToggle("abright", tele.autoBright);
      menuSetAutoBrightLabel(tele.autoBright);
      break;
    case 11:
      otaStart();
      otaStartMs = millis();
      mode = Mode::OTA;
      break;
    default: mode = Mode::SCREENS; break;  // «Выход»
  }
}

static void handleEvent(BtnEvent e) {
  switch (mode) {
    case Mode::SCREENS:
      if (e == BtnEvent::SHORT_MODE) screenIdx = (screenIdx + 1) % 4;
      else if (e == BtnEvent::LONG_MODE) viewCycle();  // цифры→+шкала→стрелка
      else if (e == BtnEvent::LONG_SET) { mode = Mode::MENU; menuCursor = 0; }
      else if (e == BtnEvent::LONG_BACK) hudToggle();  // проекция вкл/выкл
      break;

    case Mode::MENU:
      if (e == BtnEvent::SHORT_MODE) menuCursor = (menuCursor + 1) % MENU_COUNT;
      else if (e == BtnEvent::SHORT_SET) menuSelect();
      else if (e == BtnEvent::SHORT_BACK) mode = Mode::SCREENS;
      break;

    case Mode::CONFIRM:
      if (e == BtnEvent::SHORT_SET) {
        if (confirmAct == 1) odoResetTrip();
        else                 svcMarkDone();
        mode = Mode::SCREENS;
      } else if (e == BtnEvent::SHORT_BACK) {
        mode = Mode::MENU;
      }
      break;

    case Mode::ODO_EDIT:
      if (e == BtnEvent::SHORT_MODE) {
        odoDigits[odoPos] = (odoDigits[odoPos] == '9') ? '0' : odoDigits[odoPos] + 1;
      } else if (e == BtnEvent::SHORT_SET) {
        if (++odoPos >= 6) {
          odoSetTotal(atof(odoDigits));
          mode = Mode::MENU;
        }
      } else if (e == BtnEvent::SHORT_BACK) {
        mode = Mode::MENU;
      }
      break;

    case Mode::OTA:
      if (e == BtnEvent::SHORT_BACK) {
        otaStop();
        mode = Mode::MENU;
      }
      break;
  }
}

void uiInit() {
  {
    Preferences p;
    p.begin("speedo", true);
    tele.needleMode = p.getBool("needle", false);
    tele.barScale   = p.getBool("bar", true);
    tele.clockShow  = p.getBool("clock", true);
    tele.hudMode    = p.getBool("hud", false);
    tele.autoHud    = p.getBool("ahud", false);
    tele.autoBright = p.getBool("abright", true);
    brightIdx       = p.getUChar("bright", 0);
    if (brightIdx >= CONTRAST_COUNT) brightIdx = 0;
    displayContrast(CONTRASTS[brightIdx]);   // применить сохранённую
    menuSetBrightLabel(CONTRAST_COUNT - brightIdx);
    p.end();
    menuSetDialLabel(tele.needleMode);
    menuSetBarLabel(tele.barScale);
    menuSetClockLabel(tele.clockShow);
    menuSetHudLabel(tele.hudMode);
    menuSetAutoHudLabel(tele.autoHud);
    menuSetAutoBrightLabel(tele.autoBright);
    menuSetSvcLabel(svcGetInterval());
  }
  bMode.pin = PIN_BTN_MODE;
  bSet.pin  = PIN_BTN_SET;
  bBack.pin = PIN_BTN_BACK;
  pinMode(bMode.pin, INPUT_PULLUP);
  pinMode(bSet.pin,  INPUT_PULLUP);
  pinMode(bBack.pin, INPUT_PULLUP);
}

void uiTick() {
  BtnEvent e = pollButtons();
  if (e != BtnEvent::NONE) { handleEvent(e); render(); return; }

  // раз в 30 с: автояркость и авто-HUD по высоте солнца
  static uint32_t envT = 0;
  static bool     lastNight = false;
  if (millis() - envT >= 30000) {
    envT = millis();
    if (tele.autoBright)          // ночью приглушаем; днём — выбор «Яркость»
      displayContrast(tele.isNight ? 8 : CONTRASTS[brightIdx]);
    if (tele.autoHud && tele.isNight != lastNight) {
      hudSet(tele.isNight);       // авто-HUD только на переходе день↔ночь
    }
    lastNight = tele.isNight;
  }

  // авто-выход из режима OTA по таймауту
  if (mode == Mode::OTA && tele.otaActive &&
      millis() - otaStartMs > OTA_TIMEOUT_MS && tele.otaProgress < 0) {
    otaStop();
    mode = Mode::MENU;
  }

  if (millis() - lastDraw >= REDRAW_MS) render();
}
