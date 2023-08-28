#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>

#include <MAX6675.h>
#include "pikachu.h"
#include "menu.h"

#define ENCODER_CLK A2
#define ENCODER_DT A3
#define ENCODER_BUTTON 2
#define HEATGUN_FAN 3
#define SOLDERING_IRON 5
#define HEATGUN_ELEMENT 6
#define CS0 10
#define CS1 8

float celsius0;
float celsius1;

enum menuItems = {
    NOTHING,
    SOLDERING_IRON_SET_TEMP,
    HEATGUN_ELEMENT_SET_TEMP,
    HEATGUN_FAN_SET_SPEED
}

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
    attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON), checkButton, CHANGE);

    pinMode(HEATGUN_FAN, OUTPUT);
    analogWrite(HEATGUN_FAN, 255);

    pinMode(HEATGUN_ELEMENT, OUTPUT);
    analogWrite(HEATGUN_ELEMENT, 0);

    pinMode(SOLDERING_IRON, OUTPUT);

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally

    // animatePikachu(3, 2);

    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
} 

void loop()
{

    celsius0 = tcouple0.readTempC();
    celsius1 = tcouple1.readTempC();

    if (celsius0 < 50)
    {
        analogWrite(SOLDERING_IRON, 255);
    }
    else
    {
        analogWrite(SOLDERING_IRON, 0);
    }
    if (celsius1 < 50)
    {
        analogWrite(HEATGUN_ELEMENT, 255);
    }
    else
    {
        analogWrite(HEATGUN_ELEMENT, 0);
    }

    analogWrite(HEATGUN_FAN, 250);


        display.clearDisplay();
        display.drawFastVLine(64, 0, 54, SSD1306_WHITE);
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
        display.setCursor(25, 56);
        display.println("100%");
        display.setCursor(60, 56);
        display.println("Set: ");
        display.setCursor(85, 56);
        display.println("100%");
        
        display.setTextSize(2);
        display.setCursor(0, 10);
        display.println("50");
        display.setCursor(69, 10);
        display.println("50");
        display.setCursor(0, 36);
        display.println((int)(celsius0));
        display.setCursor(69, 36);
        display.println((int)(celsius1));
        display.display();

}

void animatePikachu() {
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
            delay(140*speed);
        }
    }
    
}

void checkButton()
{
    static bool prevState = false;

    if (prevState == EncoderButtonState)
    {
        EncoderButtonState = EncoderButtonState ? false : true;
    }
    else
    {
        prevState = prevState ? false : true;
    }
}
