#include "control.h"
#include "pid.h"
#include "buzzer.h"
#include <MAX6675.h>
#include <math.h>
#include <SPI.h>

static MAX6675 tcouple0(CS0);
static MAX6675 tcouple1(CS1);

bool tempInvalid(float t)
{
    return isnan(t) || t < -10.0f || t > 1024.0f;
}

static bool debounceLow(uint8_t pin, bool &stable, bool &lastRaw, unsigned long &lastChange)
{
    bool raw = digitalRead(pin) == LOW;
    if (raw != lastRaw)
    {
        lastRaw = raw;
        lastChange = millis();
    }
    else if ((millis() - lastChange) >= HOLDER_DEBOUNCE_MS)
    {
        stable = raw;
    }
    return stable;
}

static void updateHolders()
{
    static bool ironStable = false, gunStable = false;
    static bool ironRaw = false, gunRaw = false;
    static unsigned long ironMs = 0, gunMs = 0;
    bool prevIron = ironInHolder;
    bool prevGun = gunInCradle;
    ironInHolder = debounceLow(SOLDERING_IRON_HOLDER, ironStable, ironRaw, ironMs);
    gunInCradle = debounceLow(REED_SWITCH, gunStable, gunRaw, gunMs);
    if (ironInHolder != prevIron)
        pidReset(ironPid);
    if (gunInCradle != prevGun)
        pidReset(gunPid);
}

static void updateEffectiveSetpoints()
{
    if (!heatingArmed)
    {
        effIronSet = 0;
        effGunSet = 0;
        return;
    }
    effIronSet = solderingIronSetTemp;
    if (ironInHolder && solderingIronSetTemp > SLEEP_TEMP)
        effIronSet = SLEEP_TEMP;
    effGunSet = heatGunSetTemp;
    if (gunInCradle && heatGunSetTemp > SLEEP_TEMP)
        effGunSet = SLEEP_TEMP;
    if (ironTr.fault)
        effIronSet = 0;
    if (gunTr.fault)
        effGunSet = 0;
}

static float emaFilter(float raw, float &filt, bool &inited)
{
    if (tempInvalid(raw))
        return filt;
    if (!inited)
    {
        filt = raw;
        inited = true;
    }
    else
    {
        filt = filt * 0.75f + raw * 0.25f;
    }
    return filt;
}

static void updateThermalRunaway(TrState &tr, float temp, int effSet, uint8_t duty)
{
    if (tr.fault)
        return;

    if (tempInvalid(temp))
    {
        if (++tr.invalidStreak >= TR_BAD_STREAK)
        {
            tr.fault = true;
            tr.tracking = false;
        }
        return;
    }
    tr.invalidStreak = 0;

    if (effSet > 0 && temp > effSet + TR_OVER_DELTA)
    {
        if (++tr.overshootStreak >= TR_BAD_STREAK)
        {
            tr.fault = true;
            tr.tracking = false;
        }
        return;
    }
    tr.overshootStreak = 0;

    bool needRise = heatingArmed && effSet > 0 && duty == 255 &&
                    temp < (effSet - TR_AT_BAND);
    if (!needRise)
    {
        tr.tracking = false;
        return;
    }
    if (!tr.tracking)
    {
        tr.tracking = true;
        tr.windowStart = millis();
        tr.windowStartTemp = temp;
        return;
    }
    if (millis() - tr.windowStart >= TR_WINDOW_MS)
    {
        if ((temp - tr.windowStartTemp) < TR_MIN_RISE)
            tr.fault = true;
        tr.windowStart = millis();
        tr.windowStartTemp = temp;
        if (tr.fault)
            tr.tracking = false;
    }
}

static bool pollThermocouples()
{
    static unsigned long lastTcMs = 0;
    if (millis() - lastTcMs < TC_POLL_MS)
        return false;
    lastTcMs = millis();
    float r0 = tcouple0.readTempC();
    float r1 = tcouple1.readTempC();
    celsius0 = r0;
    celsius1 = r1;
    emaFilter(r0, filtIron, filtIronInit);
    emaFilter(r1, filtGun, filtGunInit);
    return true;
}

void controlInit()
{
    pinMode(REED_SWITCH, INPUT_PULLUP);
    pinMode(SOLDERING_IRON_HOLDER, INPUT_PULLUP);
    pinMode(HEATGUN_FAN, OUTPUT);
    pinMode(HEATGUN_ELEMENT, OUTPUT);
    pinMode(SOLDERING_IRON, OUTPUT);
    analogWrite(SOLDERING_IRON, 0);
    analogWrite(HEATGUN_ELEMENT, 0);
    analogWrite(HEATGUN_FAN, 0);
}

void controlTick()
{
    updateHolders();
    bool tcFresh = pollThermocouples();
    updateEffectiveSetpoints();

    if (!tcFresh)
        return;

    ironDuty = ironTr.fault ? 0 : pidCompute(ironGains, ironPid, filtIron, effIronSet);
    gunDuty = gunTr.fault ? 0 : pidCompute(gunGains, gunPid, filtGun, effGunSet);
    updateThermalRunaway(ironTr, filtIron, effIronSet, ironDuty);
    updateThermalRunaway(gunTr, filtGun, effGunSet, gunDuty);
    updateEffectiveSetpoints();
    if (ironTr.fault)
    {
        ironDuty = 0;
        pidReset(ironPid);
    }
    if (gunTr.fault)
    {
        gunDuty = 0;
        pidReset(gunPid);
    }

    analogWrite(SOLDERING_IRON, ironDuty);
    analogWrite(HEATGUN_ELEMENT, gunDuty);

    bool fanOn = (filtGun > 50.0f) || (heatingArmed && heatGunSetTemp > 0 && !gunTr.fault);
    if (gunTr.fault && filtGun > 50.0f)
        fanOn = true;
    analogWrite(HEATGUN_FAN, fanOn ? heatGunFanSetSpeed : 0);

    ironAtTemp = heatingArmed && effIronSet > 0 && !ironTr.fault &&
                 filtIronInit && !tempInvalid(filtIron) &&
                 fabsf(filtIron - effIronSet) <= (float)TR_AT_BAND;
    gunAtTemp = heatingArmed && effGunSet > 0 && !gunTr.fault &&
                filtGunInit && !tempInvalid(filtGun) &&
                fabsf(filtGun - effGunSet) <= (float)TR_AT_BAND;
    if ((ironAtTemp && !ironAtTempPrev) || (gunAtTemp && !gunAtTempPrev))
        buzzReady();
    ironAtTempPrev = ironAtTemp;
    gunAtTempPrev = gunAtTemp;
}
