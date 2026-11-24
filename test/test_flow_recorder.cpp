// The curve of the flow In and Out (FlowRecorder.cpp): one sample every CURVE_SAMPLE_MS with the water that went through each sensor since
// the previous sample, in tenths of ml.
#include <cstdio>
#include "src/coffee/FlowRecorder.h"
#include "TestSupport.h"

int main() {
  FlowRecorder r;
  CHECK(!r.recording() && r.count() == 0);

  // nothing is taken before the first interval, then one sample per interval
  r.begin(1000);
  CHECK(r.recording());
  r.update(1000, 0.0f, 0.0f);
  r.update(1000 + CURVE_SAMPLE_MS - 1, 3.2f, 0.4f);
  CHECK(r.count() == 0);
  r.update(1000 + CURVE_SAMPLE_MS, 3.2f, 0.4f);
  CHECK(r.count() == 1 && r.samples()[0] == 32 && r.samples()[1] == 4);
  r.update(1000 + CURVE_SAMPLE_MS + 10, 9.9f, 9.9f);          // not due yet
  CHECK(r.count() == 1);
  r.update(1000 + 2 * CURVE_SAMPLE_MS, 7.9f, 0.4f);           // In +4.7 ml, Out unchanged
  CHECK(r.count() == 2 && r.samples()[2] == 47 && r.samples()[3] == 0);

  // a late loop: the samples that were due come at once, the water is in the first one
  r.update(1000 + 5 * CURVE_SAMPLE_MS + 20, 10.9f, 1.4f);
  CHECK(r.count() == 5 && r.samples()[4] == 30 && r.samples()[5] == 10);
  CHECK(r.samples()[6] == 0 && r.samples()[7] == 0 && r.samples()[8] == 0 && r.samples()[9] == 0);

  // a volume that goes back (noise) gives 0 and is not counted twice
  r.update(1000 + 6 * CURVE_SAMPLE_MS, 10.9f, 1.3f);
  CHECK(r.count() == 6 && r.samples()[10] == 0 && r.samples()[11] == 0);
  r.update(1000 + 7 * CURVE_SAMPLE_MS, 11.0f, 1.5f);
  CHECK(r.count() == 7 && r.samples()[12] == 1 && r.samples()[13] == 1);   // +0.1 and +0.1 since the highest values (10.9 and 1.4)

  // more than 25.5 ml in one sample (not possible with the pump): cut, the rest comes with the next sample
  r.update(1000 + 8 * CURVE_SAMPLE_MS, 51.0f, 1.5f);
  CHECK(r.samples()[14] == 255);
  r.update(1000 + 9 * CURVE_SAMPLE_MS, 51.0f, 1.5f);
  CHECK(r.samples()[16] == 145);                                          // 510 - 110 - 255
  CHECK(r.samples()[17] == 0);

  // stop: nothing more is taken, the curve stays
  r.stop();
  r.update(1000 + 20 * CURVE_SAMPLE_MS, 60.0f, 5.0f);
  CHECK(!r.recording() && r.count() == 9);

  // the volumes of a new extraction start again at 0: begin() starts a new curve
  r.begin(5000);
  CHECK(r.recording() && r.count() == 0);
  r.update(5000 + CURVE_SAMPLE_MS, 1.0f, 0.0f);
  CHECK(r.count() == 1 && r.samples()[0] == 10 && r.samples()[1] == 0);

  // a negative volume (a sensor that was reset) is 0
  r.begin(0);
  r.update(CURVE_SAMPLE_MS, -2.0f, -1.0f);
  CHECK(r.count() == 1 && r.samples()[0] == 0 && r.samples()[1] == 0);

  // the curve is full after CURVE_SAMPLES samples: the end of a very long extraction is not recorded
  r.begin(0);
  for (int i = 1; i <= CURVE_SAMPLES + 20; i++) r.update((uint32_t)i * CURVE_SAMPLE_MS, i * 0.1f, 0.0f);
  CHECK(r.count() == CURVE_SAMPLES && !r.recording());
  CHECK(r.samples()[2 * (CURVE_SAMPLES - 1)] == 1);

  // the clock wraps (49 days): the intervals are still counted
  r.begin(0xFFFFFF00UL);
  r.update(0xFFFFFF00UL + CURVE_SAMPLE_MS, 1.0f, 0.0f);   // wraps past 2^32
  CHECK(r.count() == 1 && r.samples()[0] == 10);

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails);
  return fails != 0;
}
