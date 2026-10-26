#include "pid.h"
#include <EEPROM.h>
#include <string.h>

const PidGains IRON_DEFAULT = {400, 15, 800};
const PidGains GUN_DEFAULT = {300, 12, 600};

void pidReset(PidRuntime &rt)
{
    rt.integral = 0;
    rt.lastErr = 0;
}

uint8_t pidCompute(const PidGains &g, PidRuntime &rt, float temp, int set)
{
    if (set <= 0)
    {
        pidReset(rt);
        rt.lastSet = 0;
        return 0;
    }
    if (set != rt.lastSet)
    {
        pidReset(rt);
        rt.lastSet = set;
    }

    float err = (float)set - temp;
    float dErr = err - rt.lastErr;
    float out = (g.kp * err + g.ki * rt.integral + g.kd * dErr) / 100.0f;

    uint8_t pwm;
    if (out >= 255.0f)
        pwm = 255;
    else if (out <= 0.0f)
        pwm = 0;
    else
        pwm = (uint8_t)(out + 0.5f);

    bool satHi = (pwm == 255 && err > 0);
    bool satLo = (pwm == 0 && err < 0);
    if (!satHi && !satLo)
    {
        rt.integral += err;
        if (rt.integral > PID_I_MAX)
            rt.integral = PID_I_MAX;
        if (rt.integral < -PID_I_MAX)
            rt.integral = -PID_I_MAX;
    }

    rt.lastErr = err;
    return pwm;
}

static uint8_t eeChecksum(const uint8_t *data, int n)
{
    uint8_t x = 0;
    for (int i = 0; i < n; i++)
        x ^= data[i];
    return x;
}

void pidLoadDefaults()
{
    ironGains = IRON_DEFAULT;
    gunGains = GUN_DEFAULT;
}

void pidSaveEeprom()
{
    uint8_t buf[16];
    buf[0] = EE_MAGIC;
    buf[1] = EE_VERSION;
    memcpy(&buf[2], &ironGains, 6);
    memcpy(&buf[8], &gunGains, 6);
    buf[14] = eeChecksum(buf, 14);
    buf[15] = 0;
    for (int i = 0; i < 15; i++)
        EEPROM.update(EE_ADDR + i, buf[i]);
}

void pidLoadEeprom()
{
    uint8_t buf[15];
    for (int i = 0; i < 15; i++)
        buf[i] = EEPROM.read(EE_ADDR + i);
    if (buf[0] != EE_MAGIC || buf[1] != EE_VERSION || buf[14] != eeChecksum(buf, 14))
    {
        pidLoadDefaults();
        return;
    }
    memcpy(&ironGains, &buf[2], 6);
    memcpy(&gunGains, &buf[8], 6);
    ironGains.kp = clampInt(ironGains.kp, PID_GAIN_MIN, PID_GAIN_MAX);
    ironGains.ki = clampInt(ironGains.ki, PID_GAIN_MIN, PID_GAIN_MAX);
    ironGains.kd = clampInt(ironGains.kd, PID_GAIN_MIN, PID_GAIN_MAX);
    gunGains.kp = clampInt(gunGains.kp, PID_GAIN_MIN, PID_GAIN_MAX);
    gunGains.ki = clampInt(gunGains.ki, PID_GAIN_MIN, PID_GAIN_MAX);
    gunGains.kd = clampInt(gunGains.kd, PID_GAIN_MIN, PID_GAIN_MAX);
}

PidGains &pidGainsFor(PidChannel ch)
{
    return ch == PID_CH_IRON ? ironGains : gunGains;
}
