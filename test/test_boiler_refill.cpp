// BoilerRefill: the refill of the boiler after the steaming mode stops when the OPV sends the water back (the boiler is full), after
// BOILER_REFILL_MAX_ML or BOILER_REFILL_MAX_MS, without water, or when stopped. The volumes In and Out are given as the sensors would.
#include <cstdio>
#include "src/Config.h"
#include "src/coffee/BoilerRefill.h"
#include "TestSupport.h"

// Runs the refill pass by pass (10 ms) with the given flows (ml/s) until it stops or `ms` elapse; returns the time it ran
static uint32_t run(BoilerRefill &r, uint32_t &now, float &in, float &out, float inRate, float outRate, uint32_t ms) {
  const uint32_t start = now;
  while (now - start < ms && r.active()) {
    now += 10;
    in += inRate * 0.01f;
    out += outRate * 0.01f;
    r.update(now, in, out);
  }
  return now - start;
}

int main() {
  // ---- not started: nothing to do
  { BoilerRefill r; CHECK(!r.active() && !r.update(0, 0, 0)); }

  // ---- the boiler fills (In alone), then the OPV opens (Out ~ In): it stops at once, the water that stayed is In - Out
  { BoilerRefill r; uint32_t now = 1000; float in = 12.0f, out = 3.0f;   // the volumes of the sensors do not start at 0
    r.begin(now, in, out);
    CHECK(r.active() && r.update(now, in, out));
    run(r, now, in, out, 5.0f, 0.0f, 4000);                     // 20 ml go into the boiler
    CHECK(r.active());
    const uint32_t ran = run(r, now, in, out, 5.0f, 4.0f, 3000);  // full: the OPV sends 80 % back
    CHECK(!r.active() && r.end() == REFILL_FULL);
    CHECK(ran <= 2 * BOILER_REFILL_WINDOW_MS + 20);
    CHECK(r.netMl() > 19.0f && r.netMl() < 23.0f);
    CHECK(!r.update(now + 10, in, out)); }

  // ---- without the Out sensor (Out stays 0): full when the flow In drops (the pressure rises), as measured: 5.2 ml/s, then 3.2
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 3.8f, 0.0f, 500);                      // the pump starts
    run(r, now, in, out, 5.2f, 0.0f, 2500);
    CHECK(r.active());
    const uint32_t ran = run(r, now, in, out, 3.2f, 0.0f, 3000);
    CHECK(!r.active() && r.end() == REFILL_FULL && ran <= 2 * BOILER_REFILL_WINDOW_MS + 20); }

  // ---- a drop before BOILER_REFILL_MIN_MS is not taken, a small one (10 %) never; the flow that rises is not a drop
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 5.2f, 0.0f, 500);
    run(r, now, in, out, 3.0f, 0.0f, 400);                      // a dip, over before 1 s
    run(r, now, in, out, 6.0f, 0.0f, 2000);                     // up again: the highest is now 6
    CHECK(r.active());
    run(r, now, in, out, 6.0f * 0.9f, 0.0f, 3000);
    CHECK(r.active()); }

  // ---- what the Out sensor counts at the very start (the steaming) does not end it
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 5.0f, 5.0f, BOILER_REFILL_MIN_MS - 100);
    CHECK(r.active());
    run(r, now, in, out, 5.0f, 0.0f, 3000);
    CHECK(r.active()); }

  // ---- a little Out (under the ratio) is not full
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 5.0f, 5.0f * BOILER_REFILL_FULL_RATIO * 0.6f, 6000);
    CHECK(r.active()); }

  // ---- the steam wand left open: everything goes through, never full: at most BOILER_REFILL_MAX_ML
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 6.0f, 0.0f, 60000);
    CHECK(r.end() == REFILL_MAX_VOLUME && r.netMl() >= BOILER_REFILL_MAX_ML && r.netMl() < BOILER_REFILL_MAX_ML + 0.2f); }

  // ---- a slow flow: at most BOILER_REFILL_MAX_MS
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    const uint32_t ran = run(r, now, in, out, 1.0f, 0.0f, 60000);
    CHECK(r.end() == REFILL_MAX_TIME && ran >= BOILER_REFILL_MAX_MS && ran <= BOILER_REFILL_MAX_MS + 10); }

  // ---- no water (the tank is empty): stopped after BOILER_REFILL_NO_FLOW_MS
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    const uint32_t ran = run(r, now, in, out, 0.1f, 0.0f, 60000);
    CHECK(r.end() == REFILL_NO_WATER && ran >= BOILER_REFILL_NO_FLOW_MS && ran <= BOILER_REFILL_NO_FLOW_MS + 10); }

  // ---- stopped from outside; a new begin() starts again from the volumes of that moment
  { BoilerRefill r; uint32_t now = 0; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 5.0f, 0.0f, 1000);
    r.stop();
    CHECK(!r.active() && r.end() == REFILL_STOPPED && !r.update(now + 10, in, out));
    r.stop();
    CHECK(r.end() == REFILL_STOPPED);
    r.begin(now, in, out);
    CHECK(r.active());
    run(r, now, in, out, 5.0f, 0.0f, 1000);
    CHECK(r.netMl() > 4.5f && r.netMl() < 5.5f); }

  // ---- millis() wraps during the refill
  { BoilerRefill r; uint32_t now = 0xFFFFFFFFUL - 2000; float in = 0, out = 0;
    r.begin(now, in, out);
    run(r, now, in, out, 5.0f, 0.0f, 4000);
    CHECK(r.active());
    run(r, now, in, out, 5.0f, 4.0f, 3000);
    CHECK(r.end() == REFILL_FULL); }

  printf(fails ? "%d FAIL\n" : "test_boiler_refill: ALL OK\n", fails);
  return fails != 0;
}
