#include <Arduino.h>
#include "pins.h"
#include "telemetry.h"
#include "gps.h"
#include "odometer.h"
#include "battery.h"
#include "display.h"
#include "ui.h"
#include "ota.h"

Telemetry tele;

static uint32_t batT = 0;
static uint32_t ledT = 0;
static bool     ledOn = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_FIX, OUTPUT);
  digitalWrite(PIN_LED_FIX, HIGH);   // LED выключен (активен LOW)

  batteryInit();
  gpsInit();
  odoInit();
  displayInit();
  uiInit();

  Serial.println("Speedo start");
}

void loop() {
  gpsUpdate();
  gpsFill(tele);

  odoUpdate(tele);
  odoFill(tele);

  if (millis() - batT >= 1000) {
    batT = millis();
    batteryFill(tele);
  }

  otaHandle();
  uiTick();

  // LED: горит при фиксе, мигает 2 Гц без фикса
  if (tele.fixValid) {
    digitalWrite(PIN_LED_FIX, LOW);
    ledOn = true;
  } else if (millis() - ledT >= 500) {
    ledT = millis();
    ledOn = !ledOn;
    digitalWrite(PIN_LED_FIX, ledOn ? LOW : HIGH);
  }
}
