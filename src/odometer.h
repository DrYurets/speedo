#pragma once
#include "telemetry.h"

void odoInit();
void odoUpdate(const Telemetry& t);  // интеграция пути + персистентность
void odoFill(Telemetry& t);
void odoResetTrip();
void odoResetTotal();
void odoSetTotal(double km);         // коррекция одометра через меню

// ТО (замена масла): интервал и одометр на момент последнего ТО — в NVS
void     svcSetInterval(uint16_t km);
uint16_t svcGetInterval();
void     svcMarkDone();              // ТО пройдено: база = текущий одометр
