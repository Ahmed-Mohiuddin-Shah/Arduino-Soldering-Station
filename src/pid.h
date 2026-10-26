#pragma once

#include "state.h"

extern const PidGains IRON_DEFAULT;
extern const PidGains GUN_DEFAULT;

void pidReset(PidRuntime &rt);
uint8_t pidCompute(const PidGains &g, PidRuntime &rt, float temp, int set);
void pidLoadDefaults();
void pidSaveEeprom();
void pidLoadEeprom();
PidGains &pidGainsFor(PidChannel ch);
