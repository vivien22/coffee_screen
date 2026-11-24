// The stop delay is NOT learned while a flow limit applies (decision of the user, 2026-11-24, may change: see the REMINDER in
// ExtractionController::finish() and CLAUDE.md). Without a limit the same extraction learns it. Built with -DFLOW_LIMIT=1.
// The water is simulated: 5 ml of In per second of ON pump, and a tail of 2.7 ml after the stop (as in test_anticipation).
#include "src/Config.h"
#include "src/coffee/ExtractionController.h"
#include <cstdio>
#include <cmath>
#include "TestSupport.h"

#define PUMP pins[4]

struct Water {
  FlowSensor f;
  ExtractionController e;
  float tail = 0;
  bool pumping = false;

  Water() : e(4, 16, f) { e.begin(); e.setLossMl(0); t = 1000; }

  // 10 ms of time. The flow reported by the sensor stays at 5 ml/s (300 ml/min) while the pump runs, whatever the slot: the stop
  // is always made at a flow above STOP_LEARN_MIN_ML_PER_S, so only the limit can prevent the learning.
  void step(uint16_t target) {
    t += 10;
    if (PUMP == 1) {
      f.in += 5.0f * 0.010f;
      tail = 2.7f;
      pumping = true;
    } else if (pumping) {
      const float d = tail < 0.020f ? tail : 0.020f;
      f.in += d;
      tail -= d;
    }
    f.rate = e.isActive() ? 300.0f : 0.0f;
    e.update(t, target);
  }

  void run(uint16_t target) {
    f.in = 0; f.out = 0; tail = 0;
    e.start(t, 0, 0, 0);
    for (int i = 0; i < 9000 && (e.isActive() || e.settlePending()); i++) {
      step(target);
    }
  }
};

int main() {
  // ---- a flow limit applies: the extraction ends, the stop delay is not touched
  {
    Water w;
    w.e.setFlowLimit(2.0f);
    w.run(30);
    CHECK(w.e.state() == EXTRACTION_FINISHED);
    CHECK(!w.e.stopDelayLearned());
    CHECK(w.e.stopDelaySeconds() == STOP_DELAY_DEFAULT_S);
  }

  // ---- no limit (0): the same extraction learns the delay
  {
    Water w;
    w.e.setFlowLimit(0.0f);
    w.run(30);
    CHECK(w.e.state() == EXTRACTION_FINISHED);
    CHECK(w.e.stopDelayLearned());
    CHECK(w.e.stopDelaySeconds() != STOP_DELAY_DEFAULT_S);
  }

  if (fails == 0) printf("test_flow_limit_learning: ALL OK\n");
  return fails ? 1 : 0;
}
