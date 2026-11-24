// The flow given to the heater as feedforward: In - Out (the water that goes back through the OPV valve is not heated), never
// below 0, 0 when nothing runs. The mock sensor gives the rates in ml/min; the controller gives ml/s.
#include "src/Config.h"
#include "src/coffee/ExtractionController.h"
#include <cstdio>
#include <cmath>
#include "TestSupport.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.001f; }

int main() {
  FlowSensor f;
  ExtractionController e(4, 16, f);
  e.begin();

  // nothing runs: no flow, whatever the sensors say
  f.rate = 300.0f; f.outRate = 120.0f;
  CHECK(near(e.inflowMlPerSec(), 0.0f));

  t = 1000;
  e.start(t, 0, 0, 0);          // no pre-infusion: RUNNING, the pump is on
  CHECK(e.isActive());

  f.rate = 300.0f; f.outRate = 0.0f;
  CHECK(near(e.inflowMlPerSec(), 5.0f));    // the OPV is shut: all the water goes through the boiler

  f.rate = 300.0f; f.outRate = 120.0f;
  CHECK(near(e.inflowMlPerSec(), 3.0f));    // 2 ml/s go back by the OPV: 3 ml/s to heat

  f.rate = 222.0f; f.outRate = 84.0f;       // the numbers of the extraction #5 of the R4 (3.7 and 1.4 ml/s)
  CHECK(near(e.inflowMlPerSec(), 2.3f));

  f.rate = 120.0f; f.outRate = 400.0f;      // more out than in (noise of the sensors): never negative
  CHECK(near(e.inflowMlPerSec(), 0.0f));

  e.stop(EXTRACTION_FINISHED);
  f.rate = 300.0f; f.outRate = 0.0f;
  CHECK(near(e.inflowMlPerSec(), 0.0f));     // the extraction is over

  if (fails == 0) printf("test_inflow: ALL OK\n");
  return fails ? 1 : 0;
}
