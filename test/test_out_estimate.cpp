// OutEstimate: the water of the OPV estimated from the drop of the flow In, without the Out sensor. The In curve of extraction #2 of the
// journal (2026-11-26, the Out sensor still worked) is replayed as the controller sees it; the estimate is compared with the Out
// that was measured. Also: nothing while not active, nothing at the free flow, reset.
#include <cstdio>
#include <cmath>
#include "src/Config.h"
#include "src/coffee/OutEstimate.h"
#include "TestSupport.h"

// Extraction #2: In and Out every 500 ms, tenths of ml (the CURVE line of the journal). Pre-infusion from 0, wait from 9 s, RUNNING
// from 13.5 s (sample 27) to the stop at 42.5 s (sample 85).
static const uint8_t IN[] = {2,23,24,26,26,26,23,25,25,24,25,25,25,24,25,25,24,23,6,1,0,0,0,0,0,0,0,3,21,24,24,24,23,22,21,20,20,18,19,
  17,17,17,18,17,17,18,18,17,19,18,18,19,18,19,19,19,19,20,19,20,19,20,20,20,20,21,20,20,20,20,21,21,20,20,21,21,20,21,20,21,20,21,21,
  20,21,21,10,3,0};
static const uint8_t OUT[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,2,14,0,0,0,0,0,0,7,0,4,2,4,7,8,3,10,5,7,9,8,8,8,9,9,9,9,8,9,8,8,8,8,7,7,
  7,7,7,6,7,7,6,6,6,6,6,6,6,6,6,6,5,6,6,5,6,6,5,6,5,6,5,6,6,5,6,5,6,6,4,4,3};
static const int SAMPLES = sizeof(IN);
static const uint32_t RUN_START_MS = 13500, RUN_END_MS = 42500;

int main() {
  CHECK(sizeof(IN) == sizeof(OUT));

  // ---- replay of #2: the estimate applies in RUNNING from OUT_ESTIMATE_SETTLE_MS, as ExtractionController does
  {
    OutEstimate e;
    e.reset();
    float in = 0.0f, measured = 0.0f, measuredActive = 0.0f;
    for (int k = 0; k < SAMPLES; k++) {
      for (int step = 0; step < 50; step++) {   // one pass every 10 ms, the water of the sample spread over it
        const uint32_t now = k * 500 + step * 10;
        in += IN[k] / 10.0f / 50.0f;
        const bool active = now >= RUN_START_MS + OUT_ESTIMATE_SETTLE_MS && now < RUN_END_MS;
        e.update(now, in, active);
        const float out = OUT[k] / 10.0f / 50.0f;
        measured += out;
        if (active) measuredActive += out;
      }
    }
    printf("#2: Out measured %.1f ml (%.1f ml while the estimate applies), estimated %.1f ml\n", measured, measuredActive, e.ml());
    CHECK(fabsf(e.ml() - measuredActive) <= 3.0f);   // what it can see: within 3 ml
    CHECK(fabsf(e.ml() - measured) <= 8.0f);         // the whole extraction (the start of each phase is not estimated)
  }

  // ---- not active: nothing, even with a low flow
  { OutEstimate e; e.reset(); float in = 0;
    for (uint32_t now = 0; now < 10000; now += 10) { in += 0.03f; e.update(now, in, false); }
    CHECK(e.ml() == 0.0f && e.rateMlPerS() == 0.0f); }

  // ---- the free flow (nothing holds the pump back): nothing sent back
  { OutEstimate e; e.reset(); float in = 0;
    for (uint32_t now = 0; now < 10000; now += 10) { in += OUT_ESTIMATE_FREE_FLOW * 1.02f / 100.0f; e.update(now, in, true); }
    CHECK(e.ml() == 0.0f); }

  // ---- a steady flow of 3.5 ml/s: GAIN x (FREE_FLOW - 3.5) every second, once the window is full
  { OutEstimate e; e.reset(); float in = 0;
    for (uint32_t now = 0; now <= 12000; now += 10) { in += 0.035f; e.update(now, in, true); }
    const float expected = OUT_ESTIMATE_GAIN * (OUT_ESTIMATE_FREE_FLOW - 3.5f);
    CHECK(fabsf(e.rateMlPerS() - expected) < 0.05f);
    CHECK(fabsf(e.ml() - expected * 10.0f) < 0.6f);   // about 10 s of the 12 (the window fills first)
    e.reset();
    CHECK(e.ml() == 0.0f && e.rateMlPerS() == 0.0f); }

  // ---- the pump hardly runs (its stop, no water): under OUT_ESTIMATE_MIN_FLOW nothing is estimated
  { OutEstimate e; e.reset(); float in = 0;
    for (uint32_t now = 0; now < 10000; now += 10) { in += 0.005f; e.update(now, in, true); }
    CHECK(e.ml() == 0.0f); }

  printf(fails ? "%d FAIL\n" : "test_out_estimate: ALL OK\n", fails);
  return fails != 0;
}
