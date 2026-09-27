# Arduino Soldering Station

PlatformIO firmware for a dual-tool soldering station on **Arduino Nano (ATmega328P)**: Hakko-style iron + heat gun, K-type thermocouples (MAX6675), 128×64 OLED, rotary encoder UI.

## Features

- **Proportional-band heater PWM** on iron and heat gun (not bang-bang)
- **Hakko-style presets**: Solder 350°C, Heat Gun 300°C, Air Rework 350°C, Full Station, Vinyl 220°C, Custom
- **Safe boot**: heaters and fan off after power/hard reset until you arm from the menu
- **Sleep**: heat-gun reed (D7) and iron holder (D4) drop that tool to 180°C while docked
- **Thermal runaway** (per channel): failed rise, overshoot, or bad TC → heater cut + `E-TR` latch; clear via menu
- **Marlin-style OLED UI**: idle HUD + scroll menus; click = select, long-press = back
- **Heat OFF** menu item: zeros setpoints and disarms heating
- **Optional buzzer** (`*_buzzer` envs): click/long-press feedback + repeating alarm on thermal fault

## Build & upload

| Env | Bootloader | Buzzer |
|-----|------------|--------|
| `nanoatmega328` | Old (57600) | off |
| `nanoatmega328new` | New (115200) | off |
| `nanoatmega328_buzzer` | Old | on (A3) |
| `nanoatmega328new_buzzer` | New | on (A3) |

**Old bootloader** (typical older Nano clones):

```bash
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload
```

**Old bootloader + buzzer:**

```bash
pio run -e nanoatmega328_buzzer
pio run -e nanoatmega328_buzzer -t upload
```

**New bootloader:**

```bash
pio run -e nanoatmega328new
pio run -e nanoatmega328new -t upload
```

**New bootloader + buzzer:**

```bash
pio run -e nanoatmega328new_buzzer
pio run -e nanoatmega328new_buzzer -t upload
```

## Pin map

| Function | Pin | Notes |
|----------|-----|--------|
| Encoder CLK / DT | D2 / D3 | |
| Encoder button | A2 | `INPUT_PULLUP` |
| Soldering iron heater PWM | D5 | |
| Iron holder sense | D4 | `INPUT_PULLUP`, short to GND = in holder |
| Heat gun element PWM | D6 | |
| Heat gun reed (cradle) | D7 | `INPUT_PULLUP`, LOW = in cradle |
| Heat gun fan PWM | D9 | |
| MAX6675 CS (iron / gun) | D10 / D8 | HW SPI: D11–D13 |
| OLED SSD1306 | I2C A4/A5 | addr `0x3C` |
| Buzzer (passive piezo) | **A3** | only in `*_buzzer` builds; other side to GND |

## UI quick guide

1. Boot → splash → idle (`OFF`); nothing heats yet
2. Click encoder → menu → **Presets** or edit Iron / Gun / Fan (editing arms heat)
3. Idle shows live °C, SET, fan %, sleep (`SLP`) / fault (`E-TR`) badges
4. Long-press → back; **Heat OFF** → safe stop; **Clear Fault** → clear runaway latch

## Libraries

Pulled via PlatformIO: Adafruit SSD1306 + GFX, Paul Stoffregen Encoder, [zhenek-kreker MAX6675](https://github.com/zhenek-kreker/MAX6675) (HW SPI, `readTempC()`).
