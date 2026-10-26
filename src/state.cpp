#include "state.h"

PidGains ironGains = {400, 15, 800};
PidGains gunGains = {300, 12, 600};
PidRuntime ironPid = {0, 0, 0};
PidRuntime gunPid = {0, 0, 0};

float celsius0 = 0;
float celsius1 = 0;
float filtIron = 0;
float filtGun = 0;
bool filtIronInit = false;
bool filtGunInit = false;

int solderingIronSetTemp = 0;
int heatGunSetTemp = 0;
int heatGunFanSetSpeed = 0;
const int fanTopPWM = FAN_PWM_MAX;

bool heatingArmed = false;
bool ironInHolder = false;
bool gunInCradle = false;
int effIronSet = 0;
int effGunSet = 0;
uint8_t ironDuty = 0;
uint8_t gunDuty = 0;
bool ironAtTemp = false;
bool gunAtTemp = false;
bool ironAtTempPrev = false;
bool gunAtTempPrev = false;

presets currentPreset = CUSTOM;

TrState ironTr = {false, false, 0, 0, 0, 0};
TrState gunTr = {false, false, 0, 0, 0, 0};

UiScreen uiScreen = UI_IDLE;
EditTarget editTarget = EDIT_IRON;
PidChannel pidMenuChannel = PID_CH_IRON;
int menuIndex = 0;
int presetIndex = 0;
int pidMenuIndex = 0;
int editValue = 0;
