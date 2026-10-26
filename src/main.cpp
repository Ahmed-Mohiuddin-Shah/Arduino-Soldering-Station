#include "config.h"
#include "state.h"
#include "pid.h"
#include "control.h"
#include "buzzer.h"
#include "ui.h"

void setup()
{
    controlInit();
    buzzInit();
    uiInit();

    heatingArmed = false;
    solderingIronSetTemp = 0;
    heatGunSetTemp = 0;
    heatGunFanSetSpeed = 0;
    pidLoadEeprom();

    uiSplash();
    uiScreen = UI_IDLE;
}

void loop()
{
    controlTick();
    uiTick();
    buzzTick();
    uiDraw();
}
