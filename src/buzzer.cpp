#include "buzzer.h"
#include "config.h"
#include "state.h"

#ifdef USE_BUZZER

struct BuzzNote
{
    uint16_t freq;
    uint16_t ms;
};

static const BuzzNote MELODY_CLICK[] PROGMEM = {{1800, 30}, {2400, 40}, {0, 0}};
static const BuzzNote MELODY_LONG[] PROGMEM = {{1400, 50}, {900, 70}, {0, 0}};
static const BuzzNote MELODY_READY[] PROGMEM = {{880, 120}, {0, 60}, {1175, 180}, {0, 0}};
static const BuzzNote MELODY_FAULT[] PROGMEM = {
    {2000, 100}, {0, 80}, {2000, 100}, {0, 80}, {2000, 100}, {0, 500}, {0, 0}};

static const BuzzNote *buzzMelody = nullptr;
static uint8_t buzzIdx = 0;
static unsigned long buzzNoteEnd = 0;
static bool buzzRepeat = false;
static bool buzzAlarmMode = false;

static void buzzStart(const BuzzNote *melody, bool repeat)
{
    buzzMelody = melody;
    buzzIdx = 0;
    buzzRepeat = repeat;
    buzzNoteEnd = 0;
    noTone(BUZZER_PIN);
}

void buzzClick()
{
    buzzAlarmMode = false;
    buzzStart(MELODY_CLICK, false);
}

void buzzLong()
{
    buzzAlarmMode = false;
    buzzStart(MELODY_LONG, false);
}

void buzzReady()
{
    if (buzzAlarmMode)
        return;
    buzzStart(MELODY_READY, false);
}

void buzzTick()
{
    if (ironTr.fault || gunTr.fault)
    {
        if (!buzzAlarmMode)
        {
            buzzAlarmMode = true;
            buzzStart(MELODY_FAULT, true);
        }
    }
    else if (buzzAlarmMode)
    {
        buzzAlarmMode = false;
        noTone(BUZZER_PIN);
        buzzMelody = nullptr;
    }

    if (!buzzMelody)
        return;
    if (millis() < buzzNoteEnd)
        return;

    uint16_t freq = pgm_read_word(&buzzMelody[buzzIdx].freq);
    uint16_t ms = pgm_read_word(&buzzMelody[buzzIdx].ms);
    if (freq == 0 && ms == 0)
    {
        if (buzzRepeat)
        {
            buzzIdx = 0;
            freq = pgm_read_word(&buzzMelody[0].freq);
            ms = pgm_read_word(&buzzMelody[0].ms);
        }
        else
        {
            noTone(BUZZER_PIN);
            buzzMelody = nullptr;
            return;
        }
    }

    if (freq == 0)
        noTone(BUZZER_PIN);
    else
        tone(BUZZER_PIN, freq);
    buzzNoteEnd = millis() + ms;
    buzzIdx++;
}

void buzzInit()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

#else

void buzzClick() {}
void buzzLong() {}
void buzzReady() {}
void buzzTick() {}
void buzzInit() {}

#endif
