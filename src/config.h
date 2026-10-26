#pragma once

#include <Arduino.h>

#define ENCODER_CLK 2
#define ENCODER_DT 3
#define ENCODER_BUTTON A2
#define HEATGUN_FAN 9
#define SOLDERING_IRON 5
#define SOLDERING_IRON_HOLDER 4
#define HEATGUN_ELEMENT 6
#define REED_SWITCH 7
#define CS0 10
#define CS1 8
#define BUZZER_PIN A3

#define TC_POLL_MS 250
#define IRON_TEMP_MIN 0
#define IRON_TEMP_MAX 450
#define GUN_TEMP_MIN 0
#define GUN_TEMP_MAX 480
#define FAN_PWM_MAX 254
#define SLEEP_TEMP 180
#define HOLDER_DEBOUNCE_MS 50
#define BTN_LONG_MS 600
#define TR_WINDOW_MS 60000UL
#define TR_MIN_RISE 5.0f
#define TR_OVER_DELTA 80
#define TR_AT_BAND 8
#define TR_BAD_STREAK 3
#define PID_I_MAX 400.0f
#define PID_GAIN_MIN 0
#define PID_GAIN_MAX 2000
#define EE_MAGIC 0xA5
#define EE_VERSION 1
#define EE_ADDR 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

inline int clampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}
