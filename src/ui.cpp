#include "ui.h"
#include "pins.h"
#include "telemetry.h"
#include "display.h"
#include "odometer.h"
#include "ota.h"

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

enum class BtnEvent : uint8_t { NONE, SHORT_MODE, SHORT_SET, SHORT_BACK, LONG_SET };

static Button bMode, bSet, bBack;

enum class Mode : uint8_t { SCREENS, MENU, CONFIRM, OTA };
static Mode    mode       = Mode::SCREENS;
static uint8_t screenIdx  = 0;   // 0=Speedo 1=Trip 2=Gps
static uint8_t menuCursor = 0;
static uint8_t confirmAct = 0;   // 0 нет, 1 сброс поездки, 2 сброс одометра
static uint8_t brightIdx  = 0;
static uint32_t lastDraw  = 0;
static uint32_t otaStartMs = 0;

static const uint8_t CONTRASTS[] = {255, 192, 128, 64};
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
  // длинное SET — проверка удержания
  if (!bSet.stableHigh && !bSet.longFired && millis() - bSet.pressedAt >= LONG_MS) {
    bSet.longFired = true;
    return BtnEvent::LONG_SET;
  }
  return BtnEvent::NONE;
}

static void render() {
  lastDraw = millis();
  switch (mode) {
    case Mode::SCREENS:
      if (screenIdx == 0)      drawSpeedo(tele);
      else if (screenIdx == 1) drawTrip(tele);
      else                     drawGps(tele);
      break;
    case Mode::MENU:
      drawMenu(menuCursor);
      break;
    case Mode::CONFIRM:
      drawConfirm(confirmAct == 1 ? "Сбросить поездку?" : "Сбросить одометр?");
      break;
    case Mode::OTA:
      drawOta(tele);
      break;
  }
}

static void menuSelect() {
  switch (menuCursor) {
    case 0: confirmAct = 1; mode = Mode::CONFIRM; break;
    case 1: confirmAct = 2; mode = Mode::CONFIRM; break;
    case 2:
      brightIdx = (brightIdx + 1) % CONTRAST_COUNT;
      displayContrast(CONTRASTS[brightIdx]);
      break;
    case 3:
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
      if (e == BtnEvent::SHORT_MODE) screenIdx = (screenIdx + 1) % 3;
      else if (e == BtnEvent::LONG_SET) { mode = Mode::MENU; menuCursor = 0; }
      break;

    case Mode::MENU:
      if (e == BtnEvent::SHORT_MODE) menuCursor = (menuCursor + 1) % MENU_COUNT;
      else if (e == BtnEvent::SHORT_SET) menuSelect();
      else if (e == BtnEvent::SHORT_BACK) mode = Mode::SCREENS;
      break;

    case Mode::CONFIRM:
      if (e == BtnEvent::SHORT_SET) {
        if (confirmAct == 1) odoResetTrip();
        else                 odoResetTotal();
        mode = Mode::SCREENS;
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

  // авто-выход из режима OTA по таймауту
  if (mode == Mode::OTA && tele.otaActive &&
      millis() - otaStartMs > OTA_TIMEOUT_MS && tele.otaProgress < 0) {
    otaStop();
    mode = Mode::MENU;
  }

  if (millis() - lastDraw >= REDRAW_MS) render();
}
