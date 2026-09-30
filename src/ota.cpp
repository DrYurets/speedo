#include "ota.h"
#include "telemetry.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

static bool active = false;

void otaStart() {
  active           = true;
  tele.otaActive   = true;
  tele.otaProgress = -1;
  tele.otaStatus   = "WiFi: " WIFI_SSID;
  tele.otaIp       = "";

  // Точка доступа: устройство само создаёт сеть, ноутбук/телефон
  // подключается к ней. Домашний WiFi не нужен — работает в машине.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);   // пароль >= 8 символов
  tele.otaIp = WiFi.softAPIP().toString();   // обычно 192.168.4.1

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
  ArduinoOTA.begin();
}

void otaHandle() {
  if (active) ArduinoOTA.handle();
}

void otaStop() {
  if (active) ArduinoOTA.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active           = false;
  tele.otaActive   = false;
  tele.otaProgress = -1;
}
