#include "ota.h"
#include "telemetry.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

static bool     active    = false;
static bool     otaBegun  = false;
static uint32_t startMs   = 0;

void otaStart() {
  active          = true;
  otaBegun        = false;
  startMs         = millis();
  tele.otaActive  = true;
  tele.otaStatus  = "WiFi: подключение...";
  tele.otaProgress = -1;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  ArduinoOTA.setHostname("speedo");
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() {
    tele.otaProgress = 0;
    tele.otaStatus   = "Заливка прошивки...";
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    tele.otaProgress = total ? (int)(done * 100 / total) : 0;
  });
  ArduinoOTA.onEnd([]() {
    tele.otaStatus   = "Готово, перезагрузка";
    tele.otaProgress = 100;
  });
  ArduinoOTA.onError([](ota_error_t) {
    tele.otaStatus   = "Ошибка OTA";
    tele.otaProgress = -1;
  });
}

void otaHandle() {
  if (!active) return;

  if (!otaBegun) {
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.begin();
      otaBegun       = true;
      tele.otaStatus = "IP: " + WiFi.localIP().toString();
    } else if (millis() - startMs > 20000) {
      tele.otaStatus = "WiFi: нет сети";
    }
    return;
  }
  ArduinoOTA.handle();
}

void otaStop() {
  if (otaBegun) ArduinoOTA.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  active          = false;
  otaBegun        = false;
  tele.otaActive  = false;
  tele.otaProgress = -1;
}
