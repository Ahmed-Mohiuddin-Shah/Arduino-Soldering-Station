#include "presets.h"
#include "pid.h"

const char *const PRESET_NAMES[] = {
    "Solder",
    "Heat Gun",
    "Air Rework",
    "Full Station",
    "Vinyl",
    "Custom"};
const int PRESET_COUNT = 6;

void applyPresetValues(presets p)
{
    currentPreset = p;
    switch (p)
    {
    case SOLDERING:
        solderingIronSetTemp = 350;
        heatGunSetTemp = 0;
        heatGunFanSetSpeed = 0;
        break;
    case HEATGUN:
        solderingIronSetTemp = 0;
        heatGunSetTemp = 300;
        heatGunFanSetSpeed = (int)(fanTopPWM * 0.60f);
        break;
    case AIR_SMD:
        solderingIronSetTemp = 0;
        heatGunSetTemp = 350;
        heatGunFanSetSpeed = (int)(fanTopPWM * 0.50f);
        break;
    case SOLDERING_AND_HEATGUN:
        solderingIronSetTemp = 350;
        heatGunSetTemp = 300;
        heatGunFanSetSpeed = (int)(fanTopPWM * 0.60f);
        break;
    case VINYL:
        solderingIronSetTemp = 0;
        heatGunSetTemp = 220;
        heatGunFanSetSpeed = (int)(fanTopPWM * 0.80f);
        break;
    case CUSTOM:
    default:
        break;
    }
    pidReset(ironPid);
    pidReset(gunPid);
}

void heatOff()
{
    solderingIronSetTemp = 0;
    heatGunSetTemp = 0;
    heatGunFanSetSpeed = 0;
    heatingArmed = false;
    currentPreset = CUSTOM;
    pidReset(ironPid);
    pidReset(gunPid);
}

void armHeat()
{
    heatingArmed = true;
}
