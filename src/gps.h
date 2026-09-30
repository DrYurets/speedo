#pragma once
#include "telemetry.h"

void gpsInit();
void gpsUpdate();              // вызывать в loop как можно чаще
void gpsFill(Telemetry& t);    // переложить распарсенное в телеметрию
