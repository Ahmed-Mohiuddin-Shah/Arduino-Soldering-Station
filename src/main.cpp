#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <MAX6675.h>
#include <math.h>
#include "pikachu.h"

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

// ponytail: P-band only (no PID); add PID if hold still drifts under load
#define PROP_BAND 30
#define TC_POLL_MS 250
#define IRON_TEMP_MIN 0
#define IRON_TEMP_MAX 450
#define GUN_TEMP_MIN 0
#define GUN_TEMP_MAX 480
#define FAN_PWM_MAX 254
#define SLEEP_TEMP 180
#define HOLDER_DEBOUNCE_MS 50
#define BTN_LONG_MS 600
#define TR_WINDOW_MS 50000UL
#define TR_MIN_RISE 8.0f
#define TR_OVER_DELTA 50

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

float celsius0 = 0;
float celsius1 = 0;
int solderingIronSetTemp = 0;
int heatGunSetTemp = 0;
int heatGunFanSetSpeed = FAN_PWM_MAX;
const int fanTopPWM = FAN_PWM_MAX;

bool heatingArmed = false;
bool ironInHolder = false;
bool gunInCradle = false;
int effIronSet = 0;
int effGunSet = 0;
uint8_t ironDuty = 0;
uint8_t gunDuty = 0;

enum presets
{
    SOLDERING,
    HEATGUN,
    AIR_SMD,
    SOLDERING_AND_HEATGUN,
    VINYL,
    CUSTOM
};
presets currentPreset = CUSTOM;

struct TrState
{
    bool fault;
    bool tracking;
    unsigned long windowStart;
    float windowStartTemp;
};
TrState ironTr = {false, false, 0, 0};
TrState gunTr = {false, false, 0, 0};

#ifdef USE_BUZZER
static unsigned long uiBeepUntil = 0;

static void buzzClick()
{
    tone(BUZZER_PIN, 2400, 40);
    uiBeepUntil = millis() + 50;
}

static void buzzLong()
{
    tone(BUZZER_PIN, 1200, 90);
    uiBeepUntil = millis() + 100;
}

static void buzzAlarmTick()
{
    if (!(ironTr.fault || gunTr.fault))
        return;
    if (millis() < uiBeepUntil)
        return;
    static unsigned long next = 0;
    static bool hi = false;
    if (millis() < next)
        return;
    hi = !hi;
    tone(BUZZER_PIN, hi ? 2200 : 1400, 120);
    next = millis() + (hi ? 160 : 450);
}

static void buzzInit()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}
#else
static void buzzClick() {}
static void buzzLong() {}
static void buzzAlarmTick() {}
static void buzzInit() {}
#endif

enum UiScreen
{
    UI_IDLE,
    UI_MENU,
    UI_EDIT,
    UI_PRESETS
};
enum EditTarget
{
    EDIT_IRON,
    EDIT_GUN,
    EDIT_FAN
};

UiScreen uiScreen = UI_IDLE;
EditTarget editTarget = EDIT_IRON;
int menuIndex = 0;
int presetIndex = 0;
int editValue = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MAX6675 tcouple0(CS0);
MAX6675 tcouple1(CS1);
Encoder encoder(ENCODER_CLK, ENCODER_DT);

static const char *const MENU_ITEMS[] = {
    "Iron Temp",
    "Heatgun Temp",
    "Fan",
    "Presets",
    "Heat OFF",
    "Clear Fault",
    "Back"};
static const int MENU_COUNT = 7;

static const char *const PRESET_NAMES[] = {
    "Solder",
    "Heat Gun",
    "Air Rework",
    "Full Station",
    "Vinyl",
    "Custom"};
static const int PRESET_COUNT = 6;

void animatePikachu(int repeat, int speed);

static int clampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static uint8_t heaterPwm(float tempC, int setC)
{
    if (setC <= 0)
        return 0;
    float err = setC - tempC;
    if (err <= 0)
        return 0;
    if (err >= PROP_BAND)
        return 255;
    return (uint8_t)(err * 255.0f / PROP_BAND);
}

static void applyPresetValues(presets p)
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
}

static void heatOff()
{
    solderingIronSetTemp = 0;
    heatGunSetTemp = 0;
    heatGunFanSetSpeed = 0;
    heatingArmed = false;
    currentPreset = CUSTOM;
}

static void armHeat()
{
    heatingArmed = true;
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
    ironInHolder = debounceLow(SOLDERING_IRON_HOLDER, ironStable, ironRaw, ironMs);
    gunInCradle = debounceLow(REED_SWITCH, gunStable, gunRaw, gunMs);
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

static bool tempInvalid(float t)
{
    return isnan(t) || t < -10.0f || t > 1024.0f;
}

static void updateThermalRunaway(TrState &tr, float temp, int effSet, uint8_t duty)
{
    if (tr.fault)
        return;
    if (tempInvalid(temp))
    {
        tr.fault = true;
        tr.tracking = false;
        return;
    }
    if (effSet > 0 && temp > effSet + TR_OVER_DELTA)
    {
        tr.fault = true;
        tr.tracking = false;
        return;
    }

    bool needRise = heatingArmed && effSet > 0 && duty > 0 && temp < (effSet - 2);
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

static void applyHeaters()
{
    ironDuty = ironTr.fault ? 0 : heaterPwm(celsius0, effIronSet);
    gunDuty = gunTr.fault ? 0 : heaterPwm(celsius1, effGunSet);
    analogWrite(SOLDERING_IRON, ironDuty);
    analogWrite(HEATGUN_ELEMENT, gunDuty);

    bool fanOn = (celsius1 > 50.0f) || (heatingArmed && heatGunSetTemp > 0 && !gunTr.fault);
    if (gunTr.fault && celsius1 > 50.0f)
        fanOn = true;
    analogWrite(HEATGUN_FAN, fanOn ? heatGunFanSetSpeed : 0);
}

static void pollThermocouples()
{
    static unsigned long lastTcMs = 0;
    if (millis() - lastTcMs < TC_POLL_MS)
        return;
    lastTcMs = millis();
    celsius0 = tcouple0.readTempC();
    celsius1 = tcouple1.readTempC();
}

// --- UI helpers ---

static void drawHudFrame()
{
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.drawFastHLine(1, 10, 126, SSD1306_WHITE);
    display.drawFastHLine(1, 52, 126, SSD1306_WHITE);
}

static void drawHeatBar(int x, int y, int w, uint8_t duty)
{
    display.drawRect(x, y, w, 4, SSD1306_WHITE);
    int fill = (duty * (w - 2)) / 255;
    if (fill > 0)
        display.fillRect(x + 1, y + 1, fill, 2, SSD1306_WHITE);
}

static const char *armLabel()
{
    if (ironTr.fault || gunTr.fault)
        return "FAULT";
    if (!heatingArmed)
        return "OFF";
    if ((ironInHolder && solderingIronSetTemp > SLEEP_TEMP) ||
        (gunInCradle && heatGunSetTemp > SLEEP_TEMP))
        return "SLP";
    if (effIronSet > 0 || effGunSet > 0)
        return "HOT";
    return "IDLE";
}

static void drawIdle()
{
    display.clearDisplay();
    drawHudFrame();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(3, 2);
    display.print(F("STATION"));
    display.setCursor(78, 2);
    display.print(armLabel());

    // IRON column
    display.setCursor(4, 13);
    display.print(F("IRON"));
    if (ironTr.fault)
    {
        display.setCursor(40, 13);
        display.print(F("E-TR"));
    }
    else if (ironInHolder && heatingArmed && solderingIronSetTemp > SLEEP_TEMP)
    {
        display.setCursor(40, 13);
        display.print(F("SLP"));
    }
    display.setTextSize(2);
    display.setCursor(4, 23);
    if (tempInvalid(celsius0))
        display.print(F("---"));
    else
        display.print((int)celsius0);
    display.setTextSize(1);
    display.setCursor(4, 42);
    display.print(F("SET "));
    display.print(solderingIronSetTemp);
    drawHeatBar(4, 48, 54, ironDuty);

    display.drawFastVLine(64, 11, 41, SSD1306_WHITE);

    // GUN column
    display.setCursor(68, 13);
    display.print(F("GUN"));
    if (gunTr.fault)
    {
        display.setCursor(100, 13);
        display.print(F("E-TR"));
    }
    else if (gunInCradle && heatingArmed && heatGunSetTemp > SLEEP_TEMP)
    {
        display.setCursor(100, 13);
        display.print(F("SLP"));
    }
    display.setTextSize(2);
    display.setCursor(68, 23);
    if (tempInvalid(celsius1))
        display.print(F("---"));
    else
        display.print((int)celsius1);
    display.setTextSize(1);
    display.setCursor(68, 42);
    display.print(F("SET "));
    display.print(heatGunSetTemp);
    drawHeatBar(68, 48, 54, gunDuty);

    display.setCursor(3, 55);
    display.print(F("FAN "));
    display.print((int)((heatGunFanSetSpeed * 100L) / fanTopPWM));
    display.print('%');
    display.setCursor(64, 55);
    display.print(PRESET_NAMES[currentPreset]);

    // blink heat ticks when armed and duty > 0
    if (heatingArmed && ((millis() / 400) & 1))
    {
        if (ironDuty > 0)
            display.fillRect(56, 14, 3, 3, SSD1306_WHITE);
        if (gunDuty > 0)
            display.fillRect(120, 14, 3, 3, SSD1306_WHITE);
    }

    display.display();
}

static void drawMenuList(const char *const *items, int count, int selected, const char *title)
{
    display.clearDisplay();
    drawHudFrame();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(3, 2);
    display.print(title);

    const int visible = 4;
    int top = selected - (visible - 1);
    if (top < 0)
        top = 0;
    if (top > count - visible)
        top = count > visible ? count - visible : 0;

    for (int i = 0; i < visible && (top + i) < count; i++)
    {
        int idx = top + i;
        int y = 14 + i * 10;
        if (idx == selected)
        {
            display.fillRect(2, y - 1, 124, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(4, y);
        display.print(idx == selected ? F(">") : F(" "));
        display.print(items[idx]);
    }
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(3, 55);
    display.print(F("click=ok  hold=back"));
    display.display();
}

static void drawEdit()
{
    display.clearDisplay();
    drawHudFrame();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(3, 2);
    if (editTarget == EDIT_IRON)
        display.print(F("EDIT IRON"));
    else if (editTarget == EDIT_GUN)
        display.print(F("EDIT GUN"));
    else
        display.print(F("EDIT FAN"));

    display.setTextSize(3);
    display.setCursor(20, 22);
    if (editTarget == EDIT_FAN)
    {
        display.print((int)((editValue * 100L) / fanTopPWM));
        display.setTextSize(1);
        display.setCursor(90, 36);
        display.print('%');
    }
    else
    {
        display.print(editValue);
        display.setTextSize(1);
        display.setCursor(90, 36);
        display.print(F("C"));
    }
    display.setCursor(3, 55);
    display.print(F("turn=set  click=save"));
    display.display();
}

static void drawUi()
{
    switch (uiScreen)
    {
    case UI_IDLE:
        drawIdle();
        break;
    case UI_MENU:
        drawMenuList(MENU_ITEMS, MENU_COUNT, menuIndex, "MENU");
        break;
    case UI_PRESETS:
        drawMenuList(PRESET_NAMES, PRESET_COUNT, presetIndex, "PRESETS");
        break;
    case UI_EDIT:
        drawEdit();
        break;
    }
}

static void enterEdit(EditTarget t)
{
    editTarget = t;
    if (t == EDIT_IRON)
        editValue = solderingIronSetTemp;
    else if (t == EDIT_GUN)
        editValue = heatGunSetTemp;
    else
        editValue = heatGunFanSetSpeed;
    uiScreen = UI_EDIT;
    encoder.readAndReset();
}

static void commitEdit()
{
    if (editTarget == EDIT_IRON)
    {
        solderingIronSetTemp = clampInt(editValue, IRON_TEMP_MIN, IRON_TEMP_MAX);
        currentPreset = CUSTOM;
        armHeat();
    }
    else if (editTarget == EDIT_GUN)
    {
        heatGunSetTemp = clampInt(editValue, GUN_TEMP_MIN, GUN_TEMP_MAX);
        currentPreset = CUSTOM;
        armHeat();
    }
    else
    {
        heatGunFanSetSpeed = clampInt(editValue, 0, FAN_PWM_MAX);
        currentPreset = CUSTOM;
        if (heatGunSetTemp > 0)
            armHeat();
    }
    uiScreen = UI_MENU;
    encoder.readAndReset();
}

static void uiLongPress()
{
    buzzLong();
    encoder.readAndReset();
    if (uiScreen == UI_EDIT || uiScreen == UI_PRESETS)
        uiScreen = UI_MENU;
    else
        uiScreen = UI_IDLE;
}

static void uiClick()
{
    buzzClick();
    encoder.readAndReset();
    if (uiScreen == UI_IDLE)
    {
        uiScreen = UI_MENU;
        menuIndex = 0;
        return;
    }
    if (uiScreen == UI_MENU)
    {
        switch (menuIndex)
        {
        case 0:
            enterEdit(EDIT_IRON);
            break;
        case 1:
            enterEdit(EDIT_GUN);
            break;
        case 2:
            enterEdit(EDIT_FAN);
            break;
        case 3:
            uiScreen = UI_PRESETS;
            presetIndex = (int)currentPreset;
            break;
        case 4:
            heatOff();
            uiScreen = UI_IDLE;
            break;
        case 5:
            ironTr.fault = false;
            gunTr.fault = false;
            ironTr.tracking = false;
            gunTr.tracking = false;
            break;
        case 6:
        default:
            uiScreen = UI_IDLE;
            break;
        }
        return;
    }
    if (uiScreen == UI_PRESETS)
    {
        applyPresetValues((presets)presetIndex);
        if (presetIndex != CUSTOM)
            armHeat();
        uiScreen = UI_IDLE;
        return;
    }
    if (uiScreen == UI_EDIT)
        commitEdit();
}

static void uiTick()
{
    // button: short click / long press on release
    static bool wasDown = false;
    static unsigned long downAt = 0;
    static bool longFired = false;
    bool down = digitalRead(ENCODER_BUTTON) == LOW;
    if (down && !wasDown)
    {
        downAt = millis();
        longFired = false;
    }
    if (down && wasDown && !longFired && (millis() - downAt >= BTN_LONG_MS))
    {
        longFired = true;
        uiLongPress();
    }
    if (!down && wasDown && !longFired && (millis() - downAt >= 30))
        uiClick();
    wasDown = down;

    int delta = encoder.readAndReset() / 2;
    if (delta == 0)
        return;

    if (uiScreen == UI_MENU)
        menuIndex = clampInt(menuIndex + delta, 0, MENU_COUNT - 1);
    else if (uiScreen == UI_PRESETS)
        presetIndex = clampInt(presetIndex + delta, 0, PRESET_COUNT - 1);
    else if (uiScreen == UI_EDIT)
    {
        editValue += delta;
        if (editTarget == EDIT_IRON)
            editValue = clampInt(editValue, IRON_TEMP_MIN, IRON_TEMP_MAX);
        else if (editTarget == EDIT_GUN)
            editValue = clampInt(editValue, GUN_TEMP_MIN, GUN_TEMP_MAX);
        else
            editValue = clampInt(editValue, 0, FAN_PWM_MAX);
    }
}

void animatePikachu(int repeat, int speed)
{
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (int i = 0; i < repeat; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            display.clearDisplay();
            display.drawBitmap(32, 0, frame[j], 64, 64, 1);
            display.display();
            delay(140 * speed);
            // heaters forced off during splash
            analogWrite(SOLDERING_IRON, 0);
            analogWrite(HEATGUN_ELEMENT, 0);
            analogWrite(HEATGUN_FAN, 0);
        }
    }
}

void setup()
{
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    pinMode(REED_SWITCH, INPUT_PULLUP);
    pinMode(SOLDERING_IRON_HOLDER, INPUT_PULLUP);
    buzzInit();

    pinMode(HEATGUN_FAN, OUTPUT);
    pinMode(HEATGUN_ELEMENT, OUTPUT);
    pinMode(SOLDERING_IRON, OUTPUT);
    analogWrite(SOLDERING_IRON, 0);
    analogWrite(HEATGUN_ELEMENT, 0);
    analogWrite(HEATGUN_FAN, 0);

    heatingArmed = false;
    solderingIronSetTemp = 0;
    heatGunSetTemp = 0;
    heatGunFanSetSpeed = 0;

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    animatePikachu(1, 1);

    uiScreen = UI_IDLE;
    encoder.readAndReset();
}

void loop()
{
    updateHolders();
    pollThermocouples();
    updateEffectiveSetpoints();

    // provisional duty for TR tracking (pre-fault)
    uint8_t ironTry = heaterPwm(celsius0, ironTr.fault ? 0 : effIronSet);
    uint8_t gunTry = heaterPwm(celsius1, gunTr.fault ? 0 : effGunSet);
    updateThermalRunaway(ironTr, celsius0, effIronSet, ironTry);
    updateThermalRunaway(gunTr, celsius1, effGunSet, gunTry);

    updateEffectiveSetpoints();
    applyHeaters();
    uiTick();
    buzzAlarmTick();
    drawUi();
}
