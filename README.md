# ESP32 Artificial Nose

Firmware for an artificial nose controller, built on a **TTGO T-Display** (ESP32 + 1.14" ST7789 TFT)
mounted on the **ESP32 Course Platform** carrier board. An **MCP4728** quad 12-bit DAC drives the
sensor, and two **0–5 V analog inputs** read it back through resistor dividers.

Built with **ESP-IDF v5.5.2** and **LVGL 9** (via `esp_lvgl_port`).

> Status: hardware bring-up. The LCD, buttons, DAC and analog inputs work.
> The nose control logic comes next (see [Roadmap](#roadmap)).

---

## Contents

- [Features](#features)
- [Hardware](#hardware)
- [Pin map](#pin-map)
- [Wiring](#wiring)
- [Building and flashing](#building-and-flashing)
- [Using the firmware](#using-the-firmware)
- [Configuration](#configuration)
- [Project structure](#project-structure)
- [Forking this project](#forking-this-project)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)

---

## Features

- **Display**: ST7789 240×135 in landscape, driven with LVGL 9.
- **Buttons**: S1–S3 on the course platform, read through a PCF8574 I/O expander. The firmware
  wakes on the PCF8574's interrupt line, debounces each press, and auto-repeats S2/S3 while held.
- **MCP4728 DAC driver**:
  - finds the chip at any address from 0x60 to 0x67
  - sets channels in millivolts, one at a time or all four in one transaction
  - reads the output registers back to confirm each write
  - reads back the reference voltage the chip is set to
  - picks the DAC up within a second if it's connected after boot
- **Analog inputs**: two 0–5 V inputs on ADC2 (IO25/IO26). Each reading averages 16 samples,
  applies the ESP32's factory ADC calibration, and is scaled back to the voltage before the divider.
- **Status screen**:
  - a green or red dot shows whether the DAC was detected
  - the DAC's reference voltage
  - all four DAC channels, with the selected one highlighted
  - both analog inputs, refreshed 5 times a second

## Hardware

| Part | Notes |
|------|-------|
| [TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display) | ESP32-D0WDQ6, 4 MB flash, ST7789 135×240, CP2104 USB-UART |
| ESP32 Course Platform | Carrier board that hosts the T-Display. It has S1–S3, a PCF8574, a DRV8833 motor driver, an SD socket, LEDs and a buzzer. Schematic: [`Documents/ESP32_CoursePlatformSch.pdf`](Documents/ESP32_CoursePlatformSch.pdf) |
| [Adafruit MCP4728](https://www.adafruit.com/product/4470) | Quad 12-bit I2C DAC, powered from **5 V** in this build (see [the I2C pull-up change](#mcp4728-on-5-v--i2c-pull-up-change)) |
| 2× resistor divider | 15 kΩ / 12 kΩ per input (optional 100 nF cap) |

Schematics for both boards are in [`Documents/`](Documents/):
- [`ESP32-TFT(6-26).pdf`](Documents/ESP32-TFT(6-26).pdf): TTGO T-Display
- [`ESP32_CoursePlatformSch.pdf`](Documents/ESP32_CoursePlatformSch.pdf): ESP32 Course Platform

## Pin map

All pins are defined in [`main/board_pins.h`](main/board_pins.h).

| Function | GPIO | Notes |
|----------|------|-------|
| LCD MOSI / SCLK | 19 / 18 | SPI2, 40 MHz |
| LCD CS / DC / RST | 5 / 16 / 23 | |
| LCD backlight | 4 | Active high (S8050 transistor) |
| I2C SDA / SCL | 21 / 22 | 100 kHz, 4.7 kΩ pull-ups (R5/R6) on the platform |
| PCF8574 INT | 39 | Input-only pin, 4.7 kΩ pull-up (R4) |
| Analog in 1 | 25 | ADC2_CH8, platform net `DAC25`, JP7 pin 1 |
| Analog in 2 | 26 | ADC2_CH9, platform net `DAC26`, JP7 pin 2 |

**I2C devices**

| Address | Device |
|---------|--------|
| `0x21` | PCF8574T (A0=1, A1=A2=0). It would be `0x39` on a PCF8574AT |
| `0x60` | MCP4728 (an MCP4728A4 would be at `0x64`; the driver checks 0x60–0x67) |

**PCF8574 port bits**

| Bit | Signal | Default |
|-----|--------|---------|
| P0 | SD_CS | 1 (SD card deselected) |
| P1–P3 | S1–S3 | 1 (inputs; a pressed button reads high) |
| P4 | M_SLEEP (DRV8833 nSLEEP) | 0 (motor driver asleep) |
| P5 | IO_EXT_0 | 1 |
| P6 / P7 | Red / yellow LED | 0 (off) |

## Wiring

### Analog inputs

Each input uses the same divider:

```
 0–5 V signal ──[ 15 kΩ ]──┬──────────────► JP7 pin 1 (IO25)   / pin 2 (IO26)
                           │        │
                       [ 12 kΩ ]  [100 nF]  (optional)
                           │        │
 signal GND ───────────────┴────────┴──── platform GND
```

- **Scaling:** 5 V in gives 2.22 V at the pin. That stays inside the ESP32's accurate ADC range
  at 12 dB attenuation (about 150–2450 mV), and the input reads up to about 5.5 V.
- **Different resistors:** keep the pin below about 2.45 V at your highest input, and update
  `AIN_R_TOP_OHM` / `AIN_R_BOT_OHM`.
- **Shared lines:** IO25/IO26 also go to the platform's two S-Video connectors. Leave them unplugged.
- **Wi-Fi:** IO25/IO26 are ADC2 pins. On the ESP32, **ADC2 can't be read while Wi-Fi is running.**

### Loopback test (DAC → ADC)

1. Connect MCP4728 **VA → IN1** and **VB → IN2**, and make sure the module and platform share GND.
2. Select channel A or B with S1 and step it with S2/S3.
3. IN1/IN2 on screen, and the `IN1=… mV IN2=… mV` log line, should follow the DAC values.

### MCP4728 on 5 V — I2C pull-up change

The Adafruit board has no regulator and no level shifter. Its SDA/SCL pull-ups go to its own VCC,
so with VCC = 5 V the ESP32's I2C pins sit at about 4.1 V. That's above their 3.6 V limit.

In this build those pull-ups are removed, so only the platform's 3.3 V pull-ups remain.

**Where they are on Adafruit's rev A layout:** both pull-ups live in **R3**, a 4×0603 resistor
array on the top side, between the left STEMMA QT connector and the chip:

| R3 row (top → bottom) | Function |
|---|---|
| 1 | Power-LED resistor |
| 2 | SCL pull-up — remove |
| 3 | SDA pull-up — remove |
| 4 | LDAC pull-down |

If you remove the whole array, **wire LDAC to GND**. To confirm the change, measure SDA→VCC and
SCL→VCC with the module unpowered: both should read open. Newer board revisions may differ, so
check with a meter first.

The MCP4728 expects a logic high of about 0.7 × VDD (3.5 V at 5 V), and a 3.3 V bus is just below
that. It works on the bench. If you see I2C errors, a BSS138 bidirectional level shifter is the
reliable fix. Running the module from 3.3 V avoids the problem but limits the outputs to 0–3.3 V.

## Building and flashing

### Prerequisites

- [ESP-IDF v5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/get-started/index.html),
  via the VS Code ESP-IDF extension or the command line
- The CP210x USB-UART driver, which the T-Display uses
- An internet connection on the first build, so the IDF component manager can download
  `esp_lvgl_port` and `lvgl`

### VS Code (ESP-IDF extension)

1. Open the project folder.
2. Edit `.vscode/settings.json`:
   - set `idf.currentSetup` to your ESP-IDF install path
   - set `idf.portWin` to your board's COM port
3. **Build**, **Flash**, then **Monitor** from the ESP-IDF status bar.

### Command line

From an ESP-IDF terminal (`export.ps1` / `export.sh` already run):

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM13 flash monitor
```

Replace `COM13` with your port (`/dev/ttyUSB0` on Linux, `/dev/cu.SLAB_USBtoUART` on macOS).
`set-target` is only needed on the first build.

A good boot log looks like this:

```
I (...) display: ST7789 240x135 ready
I (...) pcf8574: I2C device found at 0x21
I (...) pcf8574: I2C device found at 0x60
I (...) analog_in: ADC2 ready, divider 15000/12000 ohm
I (...) mcp4728: MCP4728 at 0x60
I (...) main: DAC readback A=0 B=0 C=0 D=0 mV
I (...) main: IN1=... mV IN2=... mV
```

## Using the firmware

```
┌──────────────────────────────┐
│ ● MCP4728          Vref 5.0V │   green dot = DAC detected, red = not found
│ ▌A 1.200 ▐   B 0.000         │   selected channel highlighted
│  C 0.000     D 0.000         │
│ IN1   1.19 V                 │   analog inputs, 5 Hz refresh
│ IN2   0.00 V                 │
└──────────────────────────────┘
```

| Button | Action |
|--------|--------|
| **S1** | Select the next DAC channel (A → B → C → D) |
| **S2** | Lower the selected channel by 100 mV (hold to repeat) |
| **S3** | Raise the selected channel by 100 mV (hold to repeat) |

- **Updates:** DAC values are written to the chip's output registers right away. They aren't saved
  to its EEPROM, so the outputs go back to the chip's power-on defaults after a power cycle.
- **Serial log:** every change prints a readback line, and the inputs are printed once a second.

## Configuration

| Setting | File | Default | Meaning |
|---------|------|---------|---------|
| `MCP4728_VDD_MV` | `main/mcp4728.h` | `5000` | DAC supply = full scale. Set to the measured supply for accurate output, or `3300` for a 3.3 V module |
| `AIN_R_TOP_OHM` / `AIN_R_BOT_OHM` | `main/board_pins.h` | `15000` / `12000` | Divider resistors |
| `AIN1_ADC_CHANNEL` / `AIN2_ADC_CHANNEL` / `AIN_ADC_UNIT` | `main/board_pins.h` | ADC2 CH8 / CH9 | Analog input pins |
| `DAC_STEP_MV` | `main/main.c` | `100` | Button step size |
| `REPEAT_DELAY_MS` / `REPEAT_RATE_MS` | `main/main.c` | `500` / `100` | Hold-to-repeat timing |
| `AIN_UPDATE_MS` | `main/main.c` | `200` | Analog input refresh |
| `LCD_PIXEL_CLOCK_HZ` | `main/board_pins.h` | 40 MHz | SPI clock for the TFT |

Changes to `sdkconfig.defaults` only take effect on a fresh config. Delete `sdkconfig`, or run
`idf.py fullclean`, before rebuilding.

## Project structure

```
├── CMakeLists.txt            # project definition
├── sdkconfig.defaults        # target esp32, 4 MB flash, LVGL fonts
├── dependencies.lock         # pinned managed component versions
├── Documents/                # schematics (T-Display, Course Platform)
└── main/
    ├── board_pins.h          # all pins, I2C addresses, divider values
    ├── main.c                # app: button task, DAC/ADC loop, UI updates
    ├── display.c/.h          # ST7789 + LVGL setup and status screen
    ├── io_expander.c/.h      # I2C bus init + scan, PCF8574 (buttons, outputs)
    ├── mcp4728.c/.h          # MCP4728 DAC driver
    ├── analog_in.c/.h        # ADC2 oneshot + calibration + divider scaling
    └── idf_component.yml     # esp_lvgl_port, lvgl
```

## Forking this project

### 1. Fork and clone

1. Click **Fork** at the top right of the
   [GitHub page](https://github.com/giltal/ESP32-Artificial-Nose) to copy the repo into your account.
2. Clone your fork and add this repo as `upstream`, so you can pull in later changes:

```bash
git clone https://github.com/<your-user>/ESP32-Artificial-Nose.git
```

```bash
cd ESP32-Artificial-Nose
git remote add upstream https://github.com/giltal/ESP32-Artificial-Nose.git
```

The GitHub CLI does both steps in one command:

```bash
gh repo fork giltal/ESP32-Artificial-Nose --clone
```

### 2. Adapt it to your setup

- **Serial port and IDF path:** edit `.vscode/settings.json`, or pass `-p <port>` to `idf.py`.
- **Different wiring:** change [`main/board_pins.h`](main/board_pins.h). All hardware-specific
  values live there.
- **DAC supply:** set `MCP4728_VDD_MV` to match your module.
- **A different ESP32 board:**
  1. Change the target with `idf.py set-target esp32s3` (or your chip).
  2. Update the LCD pins, the panel gap/rotation in `display_init()`, and the ADC unit/channels.
  3. On chips other than the ESP32, replace `adc_cali_create_scheme_line_fitting` in
     `analog_in.c` with the curve-fitting scheme.

### 3. Work on a branch and keep your fork up to date

Create a branch for your change:

```bash
git checkout -b my-feature
```

When you want the latest changes from this repo, fetch them:

```bash
git fetch upstream
```

Then merge them into your `main`:

```bash
git checkout main
git merge upstream/main
```

Then push your updated `main` back to your fork:

```bash
git push origin main
```

### 4. Send changes back (optional)

Push your branch to your fork, then open a pull request against `giltal/ESP32-Artificial-Nose:main`:

```bash
git push -u origin my-feature
```

Please describe the hardware you tested on in the PR.

### What not to commit

`build/`, `managed_components/` and the generated `sdkconfig` are git-ignored. Put config changes
in `sdkconfig.defaults` so they reproduce on other machines. `dependencies.lock` *is* committed,
so everyone builds with the same component versions.

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `PCF8574 not found` / buttons dead | I2C wiring. The T-Display isn't seated on the platform, or SDA/SCL are swapped |
| Red dot, "not found" | MCP4728 not powered or not on SDA/SCL. Check the address in the boot-time I2C scan |
| I2C errors with the DAC on 5 V | 5 V/3.3 V logic levels. See [the I2C pull-up change](#mcp4728-on-5-v--i2c-pull-up-change); use a level shifter |
| DAC outputs off by a constant factor | `MCP4728_VDD_MV` doesn't match the real supply voltage |
| Analog input reads noisy or random values | Input floating or divider not connected. Add the 100 nF cap; check the shared GND |
| Analog input wrong by a fixed ratio | `AIN_R_TOP_OHM` / `AIN_R_BOT_OHM` don't match the fitted resistors |
| ADC2 reads fail after adding Wi-Fi | ESP32 limitation. Move the inputs to ADC1 pins or pause Wi-Fi while sampling |
| Image mirrored or shifted | Adjust `rotation` and `esp_lcd_panel_set_gap()` in `display.c` |
| `Could not open COMxx` | Wrong port, or a serial monitor already has it open |

## Roadmap

- [ ] Artificial nose control sequence (heater/bias profiles on the DAC outputs)
- [ ] Sensor sampling and logging (SD card on the platform)
- [ ] Save DAC defaults to the MCP4728 EEPROM
- [ ] Per-input ADC calibration trim from the loopback test

## License

[MIT](LICENSE) © 2026 Gil Tal. This covers the firmware, the documentation and the ESP32 Course
Platform schematic. The TTGO T-Display schematic is LilyGO's and is included for reference only.
