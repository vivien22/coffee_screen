// FlowLimit (test, Nano R4): the pump is a pattern of slots of one mains period; the limit of the flow is reached with the flow per
// click MEASURED from the In sensor, never with the pressure. A simulated pump gives a flow per click that changes while the test
// runs (the puck gives way), and the checks are on the flow it delivers. Built with -DFLOW_LIMIT=1 by test/run.sh.
#include <cstdio>
#include <cmath>
#include "src/Config.h"
#include "src/coffee/FlowLimit.h"
#include "TestSupport.h"

// Runs the limiter for seconds of simulated time. The plant: each ON slot delivers trueMlPerClick into In (the water of a click).
// Returns the flow In measured over the last `averageSeconds` (ml/s).
struct Plant {
  explicit Plant(float mlPerClick) : trueMlPerClick(mlPerClick) {}
  float trueMlPerClick;
  float in = 0.0f;
  uint32_t now = 0;
  uint32_t onSlots = 0;   // slots ON (one click each)
  long lastSlot = -1;     // the slot whose click was counted last
};

static float run(FlowLimit &limiter, Plant &p, float seconds, float averageSeconds) {
  const uint32_t end = p.now + (uint32_t)(seconds * 1000.0f);
  const uint32_t averageFrom = end - (uint32_t)(averageSeconds * 1000.0f);
  float inAtAverage = -1.0f;
  for (; p.now < end; p.now += 5) {
    if (p.now >= averageFrom && inAtAverage < 0.0f) {
      inAtAverage = p.in;
    }
    const bool on = limiter.tick(p.now, p.in);
    const long slot = (long)(p.now / FLOW_LIMIT_SLOT_MS);   // the pump clicks once per slot, however often it is asked
    if (on && slot != p.lastSlot) {
      p.in += p.trueMlPerClick;
      p.onSlots++;
      p.lastSlot = slot;
    }
  }
  return (p.in - inAtAverage) / averageSeconds;
}

int main() {
  // 1. the limit is reached: 2.0 ml/s with a flow per click of 0.25 (the start value 0.20 is wrong, the measure corrects it)
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(2.0f, p.now, p.in);
    const float flow = run(limiter, p, 20.0f, 8.0f);
    printf("limit 2.0, true 0.25: flow %.2f ml/s, measured %.3f ml/click, duty %.3f\n", flow, limiter.mlPerClick(), limiter.duty());
    CHECK(fabsf(flow - 2.0f) < 0.2f);
    CHECK(fabsf(limiter.mlPerClick() - 0.25f) < 0.02f);
  }

  // 2. the puck gives way: the flow per click goes from 0.25 to 0.40, the limit still holds
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(2.0f, p.now, p.in);
    run(limiter, p, 10.0f, 8.0f);
    p.trueMlPerClick = 0.40f;
    const float flow = run(limiter, p, 20.0f, 8.0f);
    printf("limit 2.0, 0.25 then 0.40: flow %.2f ml/s, measured %.3f ml/click, duty %.3f\n", flow, limiter.mlPerClick(), limiter.duty());
    CHECK(fabsf(flow - 2.0f) < 0.2f);
    CHECK(fabsf(limiter.mlPerClick() - 0.40f) < 0.03f);
  }

  // 3. a low limit (0.5 ml/s, the lowest setting) is kept: about 2 clicks per second
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(0.5f, p.now, p.in);
    const float flow = run(limiter, p, 20.0f, 8.0f);
    printf("limit 0.5, true 0.25: flow %.2f ml/s, duty %.3f\n", flow, limiter.duty());
    CHECK(fabsf(flow - 0.5f) < 0.08f);
  }

  // 4. a limit above what the pump can deliver: the pump stays on (duty 1, every slot ON)
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(20.0f, p.now, p.in);
    run(limiter, p, 10.0f, 4.0f);
    CHECK(limiter.duty() == 1.0f);
    const uint32_t before = p.onSlots;
    run(limiter, p, 2.0f, 1.0f);
    const uint32_t slots = 2000 / FLOW_LIMIT_SLOT_MS;
    printf("limit 20: ON slots in 2 s %lu of %lu\n", (unsigned long)(p.onSlots - before), (unsigned long)slots);
    CHECK(p.onSlots - before + 1 >= slots);
  }

  // 5. the slots are spread evenly: over one second, the ON count is the duty (+/- 1 slot)
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(2.0f, p.now, p.in);
    run(limiter, p, 10.0f, 1.0f);
    const float duty = limiter.duty();
    const uint32_t before = p.onSlots;
    run(limiter, p, 1.0f, 1.0f);
    const float expected = duty * (1000.0f / FLOW_LIMIT_SLOT_MS);
    const float got = (float)(p.onSlots - before);
    printf("duty %.3f: ON slots in 1 s %.0f, expected %.1f\n", duty, got, expected);
    CHECK(fabsf(got - expected) <= 1.0f);
  }

  // 6. the duty is never above 1 nor below 0, whatever the limit (a negative or huge limit is not used: 0 means no limit)
  {
    FlowLimit limiter;
    Plant p{0.05f};   // a very wet puck: the flow per click is small, the duty would be above 1 without the clamp
    limiter.begin(8.0f, p.now, p.in);
    run(limiter, p, 15.0f, 4.0f);
    CHECK(limiter.duty() >= 0.0f && limiter.duty() <= 1.0f);
    CHECK(limiter.mlPerClick() >= 0.05f - 1e-6f && limiter.mlPerClick() <= 0.60f + 1e-6f);
  }

  // 7. no click, no measure: the flow per click keeps its value when the pump does not run (duty 0 for a limit of 0.0)
  {
    FlowLimit limiter;
    Plant p{0.25f};
    limiter.begin(0.0f, p.now, p.in);
    const float before = limiter.mlPerClick();
    run(limiter, p, 5.0f, 1.0f);
    CHECK(limiter.duty() == 0.0f && limiter.mlPerClick() == before && p.onSlots == 0);
  }

  if (fails == 0) printf("test_flow_limit: ALL OK\n");
  return fails ? 1 : 0;
}
