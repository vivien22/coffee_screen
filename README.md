<div align="center">

# ☕ coffee_screen

**An open firmware that turns an espresso machine into a precise, programmable one. Arduino Nano, round colour screen,
one knob.**

Built and used every day on an **Ascaso Steel Uno Pro**: it controls the boiler temperature, runs the pump and the
3-way valve, and measures the coffee that really ends in the cup.

<img src="docs/images/auto-ready.png" alt="Ready" width="200">
<img src="docs/images/auto-running.png" alt="Extraction in progress" width="200">
<img src="docs/images/steaming.png" alt="Steaming mode" width="200">

![Arduino Nano](https://img.shields.io/badge/board-Arduino%20Nano%20R3%20%2F%20R4-00979D)
![PlatformIO](https://img.shields.io/badge/build-PlatformIO-F5822A)
![Tests](https://img.shields.io/github/actions/workflow/status/vivien22/coffee_screen/test.yml?label=tests)

[Features](#-features) · [How it feels](#-how-it-feels) · [Hardware](#-hardware) · [Get started](#-get-started) ·
[Quality](#-quality) · [Documentation](docs/DOCUMENTATION.md)

</div>

> [!WARNING]
> This code is not a substitute for an independent thermal fuse / thermostat on the boiler.
> You work on a mains-powered machine with hot water and pressure: do it at your own risk.

---

## ✨ Features

### Brewing
- **Brew by volume.** Set the millilitres you want in the cup; two flow sensors (water in, water out) measure what stays in
  the system, the part that does not reach the cup is deducted, and the pump stops at the target.
- **It learns.** The water keeps coming for a moment after the pump stops: the machine measures it after each extraction and
  stops that much earlier next time, so the cup lands on the target.
- **Calibrate in one step.** Weigh the cup, type the weight: the machine corrects its estimate of the loss.
- **Pre-infusion** by volume, with an optional **soak** time, then the extraction, then an optional **flush** of the
  grouphead. Each step can be switched off by setting it to 0.
- **MANUAL mode**: the pump runs between two pushes (or for a set time), the screen shows the volume and the time.

### Temperature
- **PID boiler control** with a feedforward on the water flow: when cold water enters the boiler the heater anticipates
  instead of reacting late. PT100 probe on a MAX31865, with an offset to show the temperature of the water, not of the probe.
- **Steaming mode**: a long push switches to a dedicated mode: the machine only regulates the boiler at the steaming
  temperature (135 °C by default), no extraction, the temperature alone on the screen. Another long push goes back.
- **Two saved setpoints**: brewing and steaming temperatures are kept apart.
- **Standby** after 30 minutes without activity: heater off, a picture on the screen, any action wakes it up.

### Screen and controls
- **A round 240 x 240 colour screen** (GC9A01) that only redraws what changed: no flicker, even while a value is edited.
- **One rotary encoder with a push button**: turn to change the value shown in large digits, double push to go to the next
  setting. A separate **start / stop button** brews and stops. There is no "save": the settings are stored in the
  EEPROM as they are set.
- **A welcome picture** at start-up and in standby.

### Safeties
No-water detection (the pump stops and the screen says so), maximum duration of an extraction, probe fault detection
(heater off, `ERR` on the screen), watchdog reset. The details are in
[Safeties](docs/DOCUMENTATION.md#-safeties).

---

## 🎛 How it feels

| You do | The machine does |
|---|---|
| Turn the knob | changes the setting shown in large digits (volume, pre-infusion, wait, flush, temperature) |
| Double push | validates it and selects the next one |
| Press the start button | starts the extraction (AUTO or MANUAL); during it, ends it, then starts / stops the flush |
| Short push of the knob | switches between AUTO and MANUAL, or clears the "no water" error |
| Long push of the knob | enters / leaves the steaming mode |

<table>
<tr>
<td align="center"><img src="docs/images/auto-preinfusion.png" width="170"><br><b>Pre-infusion</b></td>
<td align="center"><img src="docs/images/auto-running.png" width="170"><br><b>Extraction</b></td>
<td align="center"><img src="docs/images/auto-flushing.png" width="170"><br><b>Flush</b></td>
<td align="center"><img src="docs/images/set-temperature.png" width="170"><br><b>Setting</b></td>
<td align="center"><img src="docs/images/steaming.png" width="170"><br><b>Steaming</b></td>
</tr>
</table>

> These pictures are not mock-ups: they are drawn by the real display code, on a computer (`./tools/render_screens.sh`).

---

## 🔌 Hardware

| | |
|---|---|
| **Board** | Arduino Nano **R3** (ATmega328P), or Nano **R4** (same wiring) |
| **Screen** | GC9A01 round display, 240 x 240 |
| **Input** | CJMCU-111 rotary encoder with push button, and a push button to start / stop |
| **Temperature** | MAX31865 amplifier + PT100 probe |
| **Water** | pump relay, 3-way valve relay, two flow sensors (In, and Out on the return of the OPV valve) |
| **Heating** | boiler relay, driven by the PID |
| **Power** | everything is powered by the external 5 V 2 A supply (the Nano, the screen, the sensors, the probe board) |

> [!WARNING]
> The USB cable must only be plugged in with the machine **switched off**, to upload the firmware or to debug.

The pin table, the notes on the power supply and on the relays, and the calibration of the flow sensors are in
[Hardware and wiring](docs/DOCUMENTATION.md#-hardware-and-wiring).

---

## 🚀 Get started

You need [PlatformIO](https://platformio.org/) and the parts above wired as described in the documentation.

```
git clone https://github.com/vivien22/coffee_screen.git
cd coffee_screen
pio run -e nanoatmega328new -t upload      # Arduino Nano R3
pio run -e nano_r4 -t upload               # Arduino Nano R4
```

Everything that depends on your machine is in `src/Config.h`: the pins, the limits and defaults of the settings, the
temperature offset, the PID gains, the calibration factors of the flow sensors and the optional start button. Each option is described in
[Configuration](docs/DOCUMENTATION.md#-configuration).

---

## 🧪 Quality

The firmware has to fit in the 30 KB of an ATmega328P and still be safe, so it is tested like software that matters:

- **Host tests**: the logic (extraction phases, stop before the target, heater, safeties, screen redraw, settings) runs on a
  computer with stubs of the Arduino libraries: `./test/run.sh`. Critical tests are checked by mutation: breaking the code
  must make them fail.
- **Pixel tests**: the screen code is run against a faithful copy of the Adafruit GFX font engine, its texts are counted pixel
  by pixel against the real library, and the pictures above are generated from it.
- **Continuous integration**: every push runs the tests, builds the firmware for the R3 and the R4 in several
  configurations, checks the memory use, and runs cppcheck.

See [Tests](docs/DOCUMENTATION.md#-tests) and [Code architecture](docs/DOCUMENTATION.md#-code-architecture).

---

## 📖 Documentation

The full description is in **[docs/DOCUMENTATION.md](docs/DOCUMENTATION.md)**:

| | |
|---|---|
| [The screens](docs/DOCUMENTATION.md#-the-screens) | every screen, with its picture |
| [Using the machine](docs/DOCUMENTATION.md#-using-the-machine) | the encoder, the two modes, the start button, the steaming mode, what is saved, standby |
| [Extraction phases](docs/DOCUMENTATION.md#-extraction-phases) | pre-infusion, wait, extraction, flush |
| [Cup volume and calibration](docs/DOCUMENTATION.md#-cup-volume-and-calibration) | how the volume is measured, the loss, the stop before the target |
| [Calibration of the flow sensors](docs/DOCUMENTATION.md#-calibration-of-the-flow-sensors) | the measured factors and the report |
| [Safeties](docs/DOCUMENTATION.md#-safeties) | every protection |
| [Hardware and wiring](docs/DOCUMENTATION.md#-hardware-and-wiring) | pins, boards, power, relays |
| [Configuration](docs/DOCUMENTATION.md#-configuration) | every option of `Config.h` |
| [Build and upload](docs/DOCUMENTATION.md#-build-and-upload) | environments, libraries, memory |

---

## 🙏 Credits

The encoder driver is based on the [CJMCU-111 rotary button demo](https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo)
by Christian Pohl (push start time, push duration and short / double / long push events were added). The screen uses the
Adafruit GFX and GC9A01A libraries, the heater the PID_v1 library.
