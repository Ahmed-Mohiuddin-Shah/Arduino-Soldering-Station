#pragma once

#include <Arduino.h>
#include "config.h"

struct PidGains
{
    int16_t kp; // ×100
    int16_t ki;
    int16_t kd;
};

struct PidRuntime
{
    float integral;
    float lastErr;
    int lastSet;
};

struct TrState
{
    bool fault;
    bool tracking;
    unsigned long windowStart;
    float windowStartTemp;
    uint8_t overshootStreak;
    uint8_t invalidStreak;
};

enum presets
{
    SOLDERING,
    HEATGUN,
    AIR_SMD,
    SOLDERING_AND_HEATGUN,
    VINYL,
    CUSTOM
};

enum UiScreen
{
    UI_IDLE,
    UI_MENU,
    UI_EDIT,
    UI_PRESETS,
    UI_PID_MENU
};

enum EditTarget
{
    EDIT_IRON,
    EDIT_GUN,
    EDIT_FAN,
    EDIT_PID_KP,
    EDIT_PID_KI,
    EDIT_PID_KD
};

enum PidChannel
{
    PID_CH_IRON,
    PID_CH_GUN
};

extern PidGains ironGains;
extern PidGains gunGains;
extern PidRuntime ironPid;
extern PidRuntime gunPid;

extern float celsius0;
extern float celsius1;
extern float filtIron;
extern float filtGun;
extern bool filtIronInit;
extern bool filtGunInit;

extern int solderingIronSetTemp;
extern int heatGunSetTemp;
extern int heatGunFanSetSpeed;
extern const int fanTopPWM;

extern bool heatingArmed;
extern bool ironInHolder;
extern bool gunInCradle;
extern int effIronSet;
extern int effGunSet;
extern uint8_t ironDuty;
extern uint8_t gunDuty;
extern bool ironAtTemp;
extern bool gunAtTemp;
extern bool ironAtTempPrev;
extern bool gunAtTempPrev;

extern presets currentPreset;
extern TrState ironTr;
extern TrState gunTr;

extern UiScreen uiScreen;
extern EditTarget editTarget;
extern PidChannel pidMenuChannel;
extern int menuIndex;
extern int presetIndex;
extern int pidMenuIndex;
extern int editValue;
