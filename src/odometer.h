#pragma once
#include "telemetry.h"

void odoInit();
void odoUpdate(const Telemetry& t);  // интеграция пути + персистентность
void odoFill(Telemetry& t);
void odoResetTrip();
void odoResetTotal();
