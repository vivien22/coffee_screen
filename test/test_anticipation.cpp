// The stop before the target (ExtractionController): the pump stops when the volume counted so far plus what is still to come
// (the flow times a delay) reaches the target; the delay is learned from the water that comes after each stop.
// The water is simulated: 5 ml/s of In while the pump runs, and a "tail" of TAIL_ML that comes after it stops, in half a second.
#include "EEPROM.h"
#include "src/Config.h"
#include "src/coffee/ExtractionController.h"
#include "src/settings/Settings.h"
#include <cstdio>
#include <cmath>
#include "TestSupport.h"

#define PUMP pins[4]

struct Water {
  FlowSensor f;
  ExtractionController e;
  float tailMl = 2.7f;              // what comes after the pump stops
  float tail = 0;                   // left to come
  bool pumping = false;
  float rateMlPerMin = 300;         // 5 ml/s

  Water() : e(4, 16, f) { e.begin(); e.setLossMl(0); t = 1000; }

  // 10 ms of time: the water follows the pump, then the controller runs
  void step(uint16_t target) {
    t += 10;
    if (PUMP == 1) {
      f.in += rateMlPerMin / 60.0f * 0.010f;
      f.rate = rateMlPerMin;
      pumping = true;
      tail = tailMl;
    } else {
      f.rate = 0;
      if (pumping) pumping = false;
      const float d = tail < 0.020f ? tail : 0.020f;    // 2 ml/s while it drains
      f.in += d;
      tail -= d;
    }
    e.update(t, target);
  }

  // One extraction up to the target and the settle after it; returns the volume In at the moment the pump stopped
  float run(uint16_t target, uint8_t flushSeconds = 0) {
    f.in = 0; f.out = 0; tail = 0;
    e.start(t, 0, 0, flushSeconds);
    float atStop = -1;
    for (int i = 0; i < 6000 && (e.isActive() || e.settlePending()); i++) {
      const bool was = (PUMP == 1);
      step(target);
      if (was && PUMP == 0 && atStop < 0) atStop = f.in;
      if (e.state() == EXTRACTION_FLUSH_WAIT && !e.settlePending()) break;
    }
    return atStop;
  }
};

int main() {
  // ---- the pump stops before the target by the delay x the flow: 0.4 s x 5 ml/s = 2 ml
  {
    Water w;
    CHECK(w.e.stopDelaySeconds() == STOP_DELAY_DEFAULT_S);
    const float atStop = w.run(100);
    CHECK(atStop > 97.9f && atStop < 98.2f);              // 100 - 2
    CHECK(w.e.state() == EXTRACTION_FINISHED && PUMP == 0);
  }

  // ---- after the settle, the final volume contains the tail (98 + 2.7 = 100.7) and it is learned: 2.7 / 5 = 0.54 s
  {
    Water w;
    const float atStop = w.run(100);
    CHECK(w.e.takeSettled());                              // the final volume was brought up to date, once
    CHECK(!w.e.takeSettled());
    CHECK(w.e.volumeMl() == 101);                           // round(98.0 + 2.7)
    CHECK(std::abs(w.e.netVolumeMl() - (atStop + 2.7f)) < 0.05f);
    CHECK(w.e.stopDelayLearned() && !w.e.stopDelayLearned());
    const float expected = STOP_DELAY_DEFAULT_S + STOP_DELAY_GAIN * (0.54f - STOP_DELAY_DEFAULT_S);
    CHECK(std::abs(w.e.stopDelaySeconds() - expected) < 0.01f);
  }

  // ---- it converges: after a few extractions the final volume is the target (100 +- 0.3 ml)
  {
    Water w;
    float first = 0, last = 0;
    for (int i = 0; i < 20; i++) {
      const float atStop = w.run(100);
      const float finalMl = atStop + w.tailMl;
      if (i == 0) first = finalMl;
      last = finalMl;
    }
    CHECK(std::abs(first - 100.7f) < 0.3f);                 // the first one is 0.7 ml over (the default delay is 0.14 s short)
    CHECK(std::abs(last - 100.0f) < 0.3f);
    CHECK(std::abs(w.e.stopDelaySeconds() - 0.54f) < 0.02f);
  }

  // ---- the tail changes (a bigger one): the delay follows it
  {
    Water w;
    for (int i = 0; i < 20; i++) w.run(100);
    w.tailMl = 4.0f;                                        // 0.8 s at 5 ml/s
    for (int i = 0; i < 25; i++) w.run(100);
    CHECK(std::abs(w.e.stopDelaySeconds() - 0.8f) < 0.03f);
  }

  // ---- no water after the stop (the simulator): the delay goes to 0
  {
    Water w;
    w.tailMl = 0;
    for (int i = 0; i < 25; i++) w.run(100);
    CHECK(w.e.stopDelaySeconds() < 0.02f);
  }

  // ---- the delay is limited: 1.5 s at most, and never more than STOP_ANTICIPATION_MAX_ML before the target
  {
    Water w;
    w.tailMl = 20.0f;                                       // absurd: 4 s at 5 ml/s
    for (int i = 0; i < 30; i++) w.run(100);
    CHECK(w.e.stopDelaySeconds() <= STOP_DELAY_MAX_S + 0.001f);
    const float atStop = w.run(100);
    CHECK(atStop >= 100.0f - STOP_ANTICIPATION_MAX_ML - 0.3f);     // never more than 6 ml early
  }
  {
    Water w;
    w.e.setStopDelaySeconds(1.5f);                          // 7.5 ml at 5 ml/s: limited to 6
    const float atStop = w.run(100);
    CHECK(atStop > 93.7f && atStop < 94.3f);
  }

  // ---- no learning from a weak flow (1 ml/s): the delay is not touched
  {
    Water w;
    w.rateMlPerMin = 60;
    w.run(30);                                              // 30 ml at 1 ml/s: 30 s
    CHECK(!w.e.stopDelayLearned() && w.e.stopDelaySeconds() == STOP_DELAY_DEFAULT_S);
    CHECK(w.e.takeSettled());                               // the final volume is still brought up to date
  }

  // ---- the flush starts inside the settle window: the pump runs again, nothing is learned
  {
    Water w;
    w.f.in = 0; w.f.out = 0; w.tail = 0;
    w.e.start(t, 0, 0, 3);
    for (int i = 0; i < 6000 && w.e.state() != EXTRACTION_FLUSH_WAIT; i++) w.step(100);
    CHECK(w.e.state() == EXTRACTION_FLUSH_WAIT && w.e.settlePending());
    for (int i = 0; i < 50; i++) w.step(100);               // 0.5 s
    w.e.startFlush(t);
    for (int i = 0; i < 300; i++) w.step(100);
    CHECK(!w.e.settlePending() && !w.e.stopDelayLearned() && w.e.stopDelaySeconds() == STOP_DELAY_DEFAULT_S);
  }

  // ---- with a flush the extraction values are final after the settle, in FLUSH_WAIT
  {
    Water w;
    w.run(100, 3);
    CHECK(w.e.state() == EXTRACTION_FLUSH_WAIT && !w.e.settlePending());
    CHECK(w.e.volumeMl() == 101);
  }

  // ---- a new extraction in the settle window ends the measure
  {
    Water w;
    w.f.in = 0; w.f.out = 0; w.tail = 0;
    w.e.start(t, 0, 0, 0);
    for (int i = 0; i < 6000 && w.e.state() != EXTRACTION_FINISHED; i++) w.step(100);
    CHECK(w.e.settlePending());
    for (int i = 0; i < 30; i++) w.step(100);
    w.e.start(t, 0, 0, 0);
    w.step(100);
    CHECK(!w.e.settlePending() && !w.e.stopDelayLearned());
  }

  // ---- the user stops the extraction (finish) in the main phase: learned too; in the pre-infusion: not
  {
    Water w;
    w.f.in = 0; w.f.out = 0; w.tail = 0;
    w.e.start(t, 0, 0, 0);
    for (int i = 0; i < 400; i++) w.step(100);              // 4 s: 20 ml
    w.e.finish(t);
    for (int i = 0; i < 400; i++) w.step(100);
    CHECK(w.e.stopDelayLearned());
  }
  {
    Water w;
    w.f.in = 0; w.f.out = 0; w.tail = 0;
    w.e.start(t, 10, 0, 0);                                 // pre-infusion of 10 ml
    for (int i = 0; i < 400; i++) w.step(100);
    CHECK(w.e.state() != EXTRACTION_PREINFUSION);           // it ended on its volume ...
    CHECK(!w.e.stopDelayLearned());                         // ... and nothing was learned from it
  }

  // ---- the settings keep the delay: a blank EEPROM and a wrong value give the default
  {
    memset(EEPROM.mem, 0xFF, 64);
    SettingsStore store;
    CHECK(store.loadStopDelay() == STOP_DELAY_DEFAULT_S);
    store.saveStopDelay(0.55f);
    CHECK(store.loadStopDelay() == 0.55f);
    store.saveStopDelay(9.0f);                              // over the limit
    CHECK(store.loadStopDelay() == STOP_DELAY_DEFAULT_S);
    store.saveStopDelay(-1.0f);
    CHECK(store.loadStopDelay() == STOP_DELAY_DEFAULT_S);
    // the settings of the user are not touched by it
    memset(EEPROM.mem, 0xFF, 64);
    store.load();
    const Settings before = store.load();
    store.saveStopDelay(0.5f);
    const Settings after = store.load();
    CHECK(before.volume == after.volume && before.loss == after.loss && before.temperature == after.temperature);
  }

  if (fails == 0) printf("test_anticipation: ALL OK\n");
  return fails ? 1 : 0;
}
