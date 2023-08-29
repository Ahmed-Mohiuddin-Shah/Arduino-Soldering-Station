#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Encoder.h>

#include <MAX6675.h>
#include "pikachu.h"
#include "menu.h"

#define ENCODER_CLK 2
#define ENCODER_DT 3
#define ENCODER_BUTTON A2
#define HEATGUN_FAN 9
#define SOLDERING_IRON 5
#define HEATGUN_ELEMENT 6
#define CS0 10
#define CS1 8

float celsius0;
float celsius1;

int solderingIronSetTemp = 0;
int heatGunSetTemp = 0;
int heatGunFanSetSpeed = 254;
unsigned const int fanTopPWM = 254;

enum menuItems
{
    NOTHING,
    SOLDERING_IRON_SET_TEMP,
    HEATGUN_ELEMENT_SET_TEMP,
    HEATGUN_FAN_SET_SPEED,
    PRESETS
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

menuItems currentMenuItem = NOTHING;

presets currentPreset = CUSTOM;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

MAX6675 tcouple0(CS0);
MAX6675 tcouple1(CS1);

Encoder encoder(ENCODER_CLK, ENCODER_DT);

int pos = 250;

bool EncoderButtonState = false;

void setup()
{
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);

    pinMode(HEATGUN_FAN, OUTPUT);
    analogWrite(HEATGUN_FAN, 255);

    pinMode(HEATGUN_ELEMENT, OUTPUT);
    analogWrite(HEATGUN_ELEMENT, 0);

    pinMode(SOLDERING_IRON, OUTPUT);

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally

    animatePikachu(2);

    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
}

void loop()
{

    logic();

    graphics();
}

void animatePikachu()
{
    animatePikachu(1, 1);
}

void animatePikachu(int repeat)
{
    animatePikachu(repeat, 1);
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
            display.drawBitmap(32, 0, frame[j], 64, 64, 1); // this displays each frame hex value
            display.display();
            delay(140 * speed);
        }
    }
}

void updateButtonState()
{

    switch (currentMenuItem)
    {
    case NOTHING:
        currentMenuItem = SOLDERING_IRON_SET_TEMP;
        break;
    case SOLDERING_IRON_SET_TEMP:
        currentMenuItem = HEATGUN_ELEMENT_SET_TEMP;
        break;
    case HEATGUN_ELEMENT_SET_TEMP:
        currentMenuItem = HEATGUN_FAN_SET_SPEED;
        break;
    case HEATGUN_FAN_SET_SPEED:
        currentMenuItem = PRESETS;
        break;
    case PRESETS:
        currentMenuItem = NOTHING;
    default:
        currentMenuItem = NOTHING;
        break;
    }
}

void graphics()
{
    display.clearDisplay();
    display.drawFastVLine(64, 0, 54, SSD1306_WHITE);
    display.drawFastVLine(50, 54, 10, SSD1306_WHITE);
    display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Set Temp:");
    display.setCursor(69, 0);
    display.println("Set Temp:");
    display.setCursor(0, 26);
    display.println("Temp:");
    display.setCursor(69, 26);
    display.println("Temp:");
    display.setCursor(0, 56);
    display.println("Fan: ");
    if (currentMenuItem == HEATGUN_FAN_SET_SPEED)
    {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.setCursor(25, 56);
    display.println(String((int)((((float)heatGunFanSetSpeed / fanTopPWM) * 100))) + "%");
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    if (currentMenuItem == SOLDERING_IRON_SET_TEMP)
    {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.setCursor(0, 10);
    display.println(solderingIronSetTemp);
    display.setTextColor(SSD1306_WHITE);
    if (currentMenuItem == HEATGUN_ELEMENT_SET_TEMP)
    {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }
    display.setCursor(69, 10);
    display.println(heatGunSetTemp);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 36);
    display.println((int)(celsius0));
    display.setCursor(69, 36);
    display.println((int)(celsius1));
    display.display();
}

void logic()
{
    if (digitalRead(ENCODER_BUTTON) == LOW)
    {
        delay(100);
        if (digitalRead(ENCODER_BUTTON) == LOW)
        {
            updateButtonState();
        }
    }

    if (currentMenuItem == SOLDERING_IRON_SET_TEMP)
    {
        solderingIronSetTemp += encoder.readAndReset() / 2;
    }
    else if (currentMenuItem == HEATGUN_ELEMENT_SET_TEMP)
    {
        heatGunSetTemp += encoder.readAndReset() / 2;
    }
    else if (currentMenuItem == HEATGUN_FAN_SET_SPEED)
    {
        heatGunFanSetSpeed += encoder.readAndReset() / 2;
        if (heatGunFanSetSpeed >= 255)
        {
            heatGunFanSetSpeed = 254;
        }
        if (heatGunFanSetSpeed <= 0)
        {
            heatGunFanSetSpeed = 0;
        }
    }

    celsius0 = tcouple0.readTempC();
    celsius1 = tcouple1.readTempC();

    if (celsius0 < solderingIronSetTemp)
    {
        analogWrite(SOLDERING_IRON, 255);
    }
    else
    {
        analogWrite(SOLDERING_IRON, 0);
    }
    if (celsius1 < heatGunSetTemp)
    {
        analogWrite(HEATGUN_ELEMENT, 255);
    }
    else
    {
        analogWrite(HEATGUN_ELEMENT, 0);
    }

    analogWrite(HEATGUN_FAN, heatGunFanSetSpeed);
}
