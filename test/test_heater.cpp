// Heater controller: relay window, full power far below the setpoint, setpoint limits, safeties,
// feedforward from the flow and its delay.
#include "src/Config.h"
#include "src/heating/HeaterController.h"
#include <cstdio>
#include "TestSupport.h"



// Runs the controller for `ms` milliseconds (every 10 ms) at a constant temperature and flow.
// Returns the part of the time during which the relay was on.
static double run(HeaterController &h, unsigned long ms, bool valid, double temperature, float flow) {
  unsigned long on = 0, total = 0;
  const unsigned long end = t + ms;
  for (; t < end; t += 10) {
    h.update(t, valid, temperature, flow);
    on += pins[PIN_HEATER_RELAY] ? 1 : 0;
    total++;
  }
  return (double)on / total;
}

int main() {
  const int RELAY = PIN_HEATER_RELAY;

  { // Setpoint limits
    HeaterController h(RELAY); h.begin();
    h.setSetpoint(200); CHECK(h.setpoint() == PROBE_SETPOINT_MAX);
    h.setSetpoint(-5);  CHECK(h.setpoint() == 0);
    h.setSetpoint(98);  CHECK(h.setpoint() == 98);
  }

  { // No setpoint (start-up value), invalid probe, disabled heater: relay off, even far below the setpoint
    t = 0; HeaterController h(RELAY); h.begin();
    CHECK(run(h, 3000, true, 20, 0) == 0);          // setpoint 0
    h.setSetpoint(98);
    CHECK(run(h, 3000, true, 20, 0) > 0.99);        // 78 degrees below: full power
    CHECK(run(h, 3000, false, 20, 0) == 0);         // sensor not valid
    h.setEnabled(false);
    CHECK(run(h, 3000, true, 20, 0) == 0);
    h.setEnabled(true);
    CHECK(run(h, 3000, true, 20, 0) > 0.99);
  }

  { // At the setpoint, nothing flows: (almost) no heating; far above: none
    t = 0; HeaterController h(RELAY); h.begin(); h.setSetpoint(98);
    run(h, 5000, true, 98, 0);
    CHECK(run(h, 5000, true, 98, 0) < 0.05);
    CHECK(run(h, 5000, true, 105, 0) == 0);
  }

  { // The PID raises the heating when the probe is below the setpoint (inside the working range)
    t = 0; HeaterController h(RELAY); h.begin(); h.setSetpoint(98);
    const double a = run(h, 10000, true, 95, 0);     // 3 degrees below: Kp x 3 = 120 ms per window, plus a little integral
    CHECK(a > 0.10 && a < 0.25);
    t = 0; HeaterController g(RELAY); g.begin(); g.setSetpoint(98);
    const double b = run(g, 10000, true, 90, 0);     // 8 degrees below: more heating
    CHECK(b > a + 0.1 && b < 0.6);
  }

  { // Feedforward: 3 ml/s, 98 - 22 degrees, gain 0.3, 1100 W: 3 x 4.186 x 76 x 0.3 / 1100 = 260 ms per window
    t = 0; HeaterController h(RELAY); h.begin(); h.setSetpoint(98);
    h.setTunings(0, 0, 0);                      // the PID does nothing: only the feedforward is seen
    h.setFeedforwardGain(0.3f); h.setHeaterPowerW(1100); h.setInletTemperature(22);
    run(h, 1000, true, 98, 3.0f);
    CHECK(h.feedforwardMs() >= 255 && h.feedforwardMs() <= 265);
    const double share = run(h, 5000, true, 98, 3.0f);
    CHECK(share > 0.24 && share < 0.28);
    h.setFeedforwardGain(0);
    run(h, 1000, true, 98, 3.0f);
    CHECK(h.feedforwardMs() == 0);
  }

  { // Delay: the feedforward starts 2 s after the water, and ends 2 s after it stopped
    t = 0; HeaterController h(RELAY); h.begin(); h.setSetpoint(98);
    h.setTunings(0, 0, 0);
    h.setFeedforwardDelayMs(2000);
    CHECK(h.feedforwardDelayMs() == 2000);
    run(h, 1000, true, 98, 0);                  // nothing flows
    run(h, 500, true, 98, 3.0f);                // water since t = 1.0 s
    CHECK(h.feedforwardMs() == 0);              // t = 1.5 s: not yet
    run(h, 1000, true, 98, 3.0f);               // t = 2.5 s
    CHECK(h.feedforwardMs() == 0);
    run(h, 700, true, 98, 3.0f);                // t = 3.2 s: the water of t = 1.2 s is used
    CHECK(h.feedforwardMs() > 200);
    run(h, 2000, true, 98, 3.0f);               // t = 5.2 s, the water goes on
    CHECK(h.feedforwardMs() > 200);
    run(h, 1000, true, 98, 0);                  // the water stopped at t = 5.2 s: still heating at t = 6.2 s
    CHECK(h.feedforwardMs() > 200);
    run(h, 1500, true, 98, 0);                  // t = 7.7 s: over
    CHECK(h.feedforwardMs() == 0);
    // the delay is limited to the history
    h.setFeedforwardDelayMs(100000);
    CHECK(h.feedforwardDelayMs() == (FEEDFORWARD_HISTORY - 1) * FEEDFORWARD_STEP_MS);
  }

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails);
  return fails != 0;
}
