#pragma once
#include "telemetry.h"

void batteryInit();
void batteryFill(Telemetry& t);   // вызывать ~раз в секунду (блокирует ~3 мс)
