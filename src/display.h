#pragma once
#include "telemetry.h"

extern const uint8_t MENU_COUNT;

void displayInit();
void displayContrast(uint8_t v);
void menuSetDialLabel(bool needle);
void menuSetBarLabel(bool on);
void menuSetClockLabel(bool on);
void menuSetHudLabel(bool on);
void menuSetSvcLabel(uint16_t km);
void displayMirror(bool on);   // HUD-зеркало

void drawSpeedo(const Telemetry& t);
void drawTrip(const Telemetry& t);
void drawService(const Telemetry& t);
void drawOdoEdit(const char* digits, uint8_t pos);
void drawGps(const Telemetry& t);
void drawMenu(uint8_t cursor);
void drawConfirm(const char* question);
void drawOta(const Telemetry& t);
