#include "ui.h"
#include "config.h"
#include "state.h"
#include "pid.h"
#include "presets.h"
#include "buzzer.h"
#include "control.h"
#include "pikachu.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static Encoder encoder(ENCODER_CLK, ENCODER_DT);

static const char *const MENU_ITEMS[] = {
    "Iron Temp",
    "Heatgun Temp",
    "Fan",
    "Presets",
    "Iron PID",
    "Gun PID",
    "Heat OFF",
    "Clear Fault",
    "Back"};
static const int MENU_COUNT = 9;

static const char *const PID_MENU_ITEMS[] = {
    "Kp",
    "Ki",
    "Kd",
    "Save",
    "Defaults",
    "Back"};
static const int PID_MENU_COUNT = 6;

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

static void printGain(int16_t g)
{
    int whole = g / 100;
    int frac = abs(g % 100);
    display.print(whole);
    display.print('.');
    if (frac < 10)
        display.print('0');
    display.print(frac);
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
    if (!filtIronInit || tempInvalid(filtIron))
        display.print(F("---"));
    else
        display.print((int)filtIron);
    display.setTextSize(1);
    display.setCursor(4, 42);
    display.print(F("SET "));
    display.print(solderingIronSetTemp);
    drawHeatBar(4, 48, 54, ironDuty);

    display.drawFastVLine(64, 11, 41, SSD1306_WHITE);

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
    if (!filtGunInit || tempInvalid(filtGun))
        display.print(F("---"));
    else
        display.print((int)filtGun);
    display.setTextSize(1);
    display.setCursor(68, 42);
    display.print(F("SET "));
    display.print(heatGunSetTemp);
    drawHeatBar(68, 48, 54, gunDuty);

    display.setCursor(3, 55);
    display.print(F("FAN "));
    display.print(fanTopPWM ? (int)((heatGunFanSetSpeed * 100L) / fanTopPWM) : 0);
    display.print('%');
    display.setCursor(64, 55);
    display.print(PRESET_NAMES[currentPreset]);

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
    else if (editTarget == EDIT_FAN)
        display.print(F("EDIT FAN"));
    else if (editTarget == EDIT_PID_KP)
        display.print(F("EDIT Kp"));
    else if (editTarget == EDIT_PID_KI)
        display.print(F("EDIT Ki"));
    else
        display.print(F("EDIT Kd"));

    display.setTextSize(3);
    display.setCursor(12, 22);
    if (editTarget == EDIT_FAN)
    {
        display.print(fanTopPWM ? (int)((editValue * 100L) / fanTopPWM) : 0);
        display.setTextSize(1);
        display.setCursor(90, 36);
        display.print('%');
    }
    else if (editTarget == EDIT_PID_KP || editTarget == EDIT_PID_KI || editTarget == EDIT_PID_KD)
    {
        display.setTextSize(2);
        display.setCursor(20, 26);
        printGain((int16_t)editValue);
    }
    else
    {
        display.print(editValue);
        display.setTextSize(1);
        display.setCursor(90, 36);
        display.print(F("C"));
    }
    display.setTextSize(1);
    display.setCursor(3, 55);
    display.print(F("turn=set  click=save"));
    display.display();
}

static void enterEdit(EditTarget t)
{
    editTarget = t;
    if (t == EDIT_IRON)
        editValue = solderingIronSetTemp;
    else if (t == EDIT_GUN)
        editValue = heatGunSetTemp;
    else if (t == EDIT_FAN)
        editValue = heatGunFanSetSpeed;
    else
    {
        PidGains &g = pidGainsFor(pidMenuChannel);
        if (t == EDIT_PID_KP)
            editValue = g.kp;
        else if (t == EDIT_PID_KI)
            editValue = g.ki;
        else
            editValue = g.kd;
    }
    uiScreen = UI_EDIT;
    encoder.readAndReset();
}

static void commitEdit()
{
    if (editTarget == EDIT_IRON)
    {
        solderingIronSetTemp = clampInt(editValue, IRON_TEMP_MIN, IRON_TEMP_MAX);
        currentPreset = CUSTOM;
        pidReset(ironPid);
        armHeat();
        uiScreen = UI_MENU;
    }
    else if (editTarget == EDIT_GUN)
    {
        heatGunSetTemp = clampInt(editValue, GUN_TEMP_MIN, GUN_TEMP_MAX);
        currentPreset = CUSTOM;
        pidReset(gunPid);
        armHeat();
        uiScreen = UI_MENU;
    }
    else if (editTarget == EDIT_FAN)
    {
        heatGunFanSetSpeed = clampInt(editValue, 0, FAN_PWM_MAX);
        currentPreset = CUSTOM;
        if (heatGunSetTemp > 0)
            armHeat();
        uiScreen = UI_MENU;
    }
    else
    {
        PidGains &g = pidGainsFor(pidMenuChannel);
        int16_t v = (int16_t)clampInt(editValue, PID_GAIN_MIN, PID_GAIN_MAX);
        if (editTarget == EDIT_PID_KP)
            g.kp = v;
        else if (editTarget == EDIT_PID_KI)
            g.ki = v;
        else
            g.kd = v;
        pidReset(pidMenuChannel == PID_CH_IRON ? ironPid : gunPid);
        uiScreen = UI_PID_MENU;
    }
    encoder.readAndReset();
}

static void uiLongPress()
{
    buzzLong();
    encoder.readAndReset();
    if (uiScreen == UI_EDIT)
    {
        uiScreen = (editTarget == EDIT_PID_KP || editTarget == EDIT_PID_KI || editTarget == EDIT_PID_KD)
                       ? UI_PID_MENU
                       : UI_MENU;
    }
    else if (uiScreen == UI_PRESETS || uiScreen == UI_PID_MENU)
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
            pidMenuChannel = PID_CH_IRON;
            pidMenuIndex = 0;
            uiScreen = UI_PID_MENU;
            break;
        case 5:
            pidMenuChannel = PID_CH_GUN;
            pidMenuIndex = 0;
            uiScreen = UI_PID_MENU;
            break;
        case 6:
            heatOff();
            uiScreen = UI_IDLE;
            break;
        case 7:
            ironTr = {false, false, 0, 0, 0, 0};
            gunTr = {false, false, 0, 0, 0, 0};
            pidReset(ironPid);
            pidReset(gunPid);
            break;
        case 8:
        default:
            uiScreen = UI_IDLE;
            break;
        }
        return;
    }
    if (uiScreen == UI_PID_MENU)
    {
        switch (pidMenuIndex)
        {
        case 0:
            enterEdit(EDIT_PID_KP);
            break;
        case 1:
            enterEdit(EDIT_PID_KI);
            break;
        case 2:
            enterEdit(EDIT_PID_KD);
            break;
        case 3:
            pidSaveEeprom();
            buzzReady();
            break;
        case 4:
            if (pidMenuChannel == PID_CH_IRON)
            {
                ironGains = IRON_DEFAULT;
                pidReset(ironPid);
            }
            else
            {
                gunGains = GUN_DEFAULT;
                pidReset(gunPid);
            }
            pidSaveEeprom();
            buzzReady();
            break;
        case 5:
        default:
            uiScreen = UI_MENU;
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

void uiInit()
{
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    encoder.readAndReset();
}

void uiSplash()
{
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (int j = 0; j < 7; j++)
    {
        display.clearDisplay();
        display.drawBitmap(32, 0, frame[j], 64, 64, 1);
        display.display();
        delay(140);
        analogWrite(SOLDERING_IRON, 0);
        analogWrite(HEATGUN_ELEMENT, 0);
        analogWrite(HEATGUN_FAN, 0);
    }
}

void uiTick()
{
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
    else if (uiScreen == UI_PID_MENU)
        pidMenuIndex = clampInt(pidMenuIndex + delta, 0, PID_MENU_COUNT - 1);
    else if (uiScreen == UI_EDIT)
    {
        if (editTarget == EDIT_PID_KP || editTarget == EDIT_PID_KI || editTarget == EDIT_PID_KD)
            editValue = clampInt(editValue + delta, PID_GAIN_MIN, PID_GAIN_MAX);
        else
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
}

void uiDraw()
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
    case UI_PID_MENU:
        drawMenuList(PID_MENU_ITEMS, PID_MENU_COUNT, pidMenuIndex,
                     pidMenuChannel == PID_CH_IRON ? "IRON PID" : "GUN PID");
        break;
    case UI_EDIT:
        drawEdit();
        break;
    }
}
