#pragma once

#include "state.h"

extern const char *const PRESET_NAMES[];
extern const int PRESET_COUNT;

void applyPresetValues(presets p);
void heatOff();
void armHeat();
