// HeaterController alone: the relay of the boiler (pin PIN_HEATER_RELAY). The PID stand-in does nothing (its output stays 0),
// so what is checked here is what the controller itself decides: full power far below the setpoint, the feedforward
// from the flow, the time-proportional window (also across the wrap of millis()), and the relay off whenever the
// temperature is not valid.
#include "EEPROM.h"
#include "src/Config.h"
#include "src/heating/HeaterController.h"
#include <cstdio>
#include "TestSupport.h"

#define HEATER pins[PIN_HEATER_RELAY]

// Runs the controller one pass per ms over `windows` PID windows starting at `start`: returns the ms with the relay on
static int onMs(HeaterController &h, uint32_t start, int windows, bool valid, double temperature, float flowMlPerSec) {
  int on = 0;
  for (uint32_t i = 0; i < (uint32_t)windows * PID_WINDOW_MS; i++) {
    t = start + i;
    h.update(t, valid, temperature, flowMlPerSec);
    on += (HEATER == 1);
  }
  return on;
}

int main() {
  // ---- begin(): the relay is off
  { HeaterController h(PIN_HEATER_RELAY); HEATER = 1; h.begin(); CHECK(HEATER == 0 && !h.isOn()); }

  // ---- invalid temperature: the relay stays off, whatever the flow and the setpoint
  { HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    CHECK(onMs(h, 0, 3, false, 20.0, 5.0f) == 0 && !h.isOn()); }

  // ---- far below the setpoint (more than PID_WORKING_RANGE): full power, all the window
  { HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    int on = onMs(h, 0, 3, true, 60.0, 0.0f);
    CHECK(on >= 3 * PID_WINDOW_MS - 2); }

  // ---- full power drops at once when the probe becomes invalid, and comes back with it
  { HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    onMs(h, 0, 1, true, 60.0, 0.0f); CHECK(h.isOn());
    t += 1; h.update(t, false, 60.0, 0.0f); CHECK(!h.isOn() && HEATER == 0);
    t += 1; h.update(t, true, 60.0, 0.0f); CHECK(h.isOn()); }

  // ---- at the setpoint, nothing flowing: the PID alone decides (0 here), no feedforward
  { HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    CHECK(onMs(h, 0, 3, true, 97.0, 0.0f) == 0); }

  // ---- feedforward: flow x 4.186 x (setpoint - inlet) x gain, in watts, over the element power; at most the whole window
  {
    // 2 ml/s: 2 x 4.186 x 75 = 628 W -> 57 % of 1100 W (with FEEDFORWARD_GAIN 1)
    HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    const float powerW = 2.0f * WATER_SPECIFIC_HEAT * (97.0f - WATER_INLET_TEMPERATURE) * FEEDFORWARD_GAIN;
    const int expected = (int)(powerW / HEATER_POWER_W * PID_WINDOW_MS + 0.5f);
    const int on = onMs(h, 0, 4, true, 97.0, 2.0f) / 4;
    CHECK(on >= expected - 2 && on <= expected + 2);
  }
  { // a flow that needs more than the element can give: saturated at the whole window, never more
    HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    CHECK(onMs(h, 0, 3, true, 97.0, 6.0f) >= 3 * PID_WINDOW_MS - 2); }
  { // a setpoint under the inlet temperature: nothing to add
    HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(WATER_INLET_TEMPERATURE - 5);
    CHECK(onMs(h, 0, 2, true, WATER_INLET_TEMPERATURE - 5, 3.0f) == 0); }

  // ---- the window across the wrap of millis() (49.7 days after the start): same duty as elsewhere
  {
    HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    const uint32_t start = 0xFFFFFFFFUL - 1500UL;   // the clock wraps in the middle of the second window
    t = start; h.update(t, true, 97.0, 2.0f);
    const int on = onMs(h, start, 4, true, 97.0, 2.0f) / 4;
    const float powerW = 2.0f * WATER_SPECIFIC_HEAT * (97.0f - WATER_INLET_TEMPERATURE) * FEEDFORWARD_GAIN;
    const int expected = (int)(powerW / HEATER_POWER_W * PID_WINDOW_MS + 0.5f);
    CHECK(on >= expected - 2 && on <= expected + 2);
  }

  // ---- back from full power to the PID range: the controller does not stay at full power
  { HeaterController h(PIN_HEATER_RELAY); t = 0; h.begin(); h.setSetpoint(97);
    onMs(h, 0, 2, true, 60.0, 0.0f);
    CHECK(onMs(h, 2000, 2, true, 97.0, 0.0f) == 0); }

  if (fails == 0) printf("test_heater: ALL OK\n");
  return fails ? 1 : 0;
}
