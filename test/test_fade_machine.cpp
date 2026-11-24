// What the machine does around the fades: an extraction that ends in the flush (the tag FLUSH, the seconds counted down), the result, the
// pushes that skip the flush and select the next parameter, a change of mode, a value set, the live temperature. After the last event and once
// everything has settled, what is on the screen must be what a complete redraw of the same state gives (nothing is left of a look that was
// interrupted). The real ScreenController and the real Display.cpp draw into the frame buffer of the GFX stub.
#include <cstdio>
#include <vector>
#include "src/Config.h"
#include "src/display/ScreenController.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

typedef std::vector<uint16_t> Picture;

static Picture snapshot() {
  return Picture(frameBuffer, frameBuffer + 240 * 240);
}

static Settings settings() {
  Settings s;
  s.temperature = 93;
  s.volume = 40;
  s.preinfusion = 10;
  s.preinfusionWait = 5;
  s.flush = 5;
  s.manualSeconds = 0;
  s.flowLimit = 0;
  s.loss = DEFAULT_LOSS_ML;
  s.manual = false;
  return s;
}

static void ready(ScreenController &sc) {
  sc.begin(settings());
  sc.finishWelcome();
  sc.showLiveTemperature(0, 92.4);
  sc.showHeaterState(true);
  t += 2000;
  sc.update(t);
}

// The state of the machine that the test plays: an extraction in its flush, the seconds left
static bool flushing = false;
static int flushLeft = 0;
static int resultMl = 60;

static void run(ScreenController &sc, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    t++;
    if (t % 1000 == 0) {
      sc.showLiveTemperature(t, 88.0 + (t / 1000 % 5) * 0.4);
      if (flushing && flushLeft > 0) {
        flushLeft--;
        sc.showFlush(resultMl, flushLeft, true);
      }
    }
    sc.update(t);
  }
}

enum Op { PUSH, MODE, UP, EXTRACT };

// An extraction ends: the result stays, and the flush starts (what the machine does through showExtractionProgress / showFlush)
static void apply(ScreenController &sc, Op op) {
  if (op == EXTRACT) {
    sc.showExtractionStarted();
    sc.showExtractionProgress(resultMl, 27);
    sc.showFlush(resultMl, 5, true);
    flushing = true;
    flushLeft = 5;
  } else if (op == PUSH) {
    if (flushing) {           // a double push skips the flush, then the next parameter is selected
      flushing = false;
      sc.showExtractionProgress(resultMl, 27);
    }
    sc.selectNextParameter();
  } else if (op == MODE) {
    if (flushing) {
      return;                 // not while an extraction is going on
    }
    sc.toggleMode();
    sc.showReady();
  } else {
    sc.adjustActiveParameter(1);
  }
}

static uint32_t seed = 987;
static uint32_t random(uint32_t n) {
  seed = seed * 1664525u + 1013904223u;
  return (seed >> 8) % n;
}

int main() {
  const uint32_t SETTLE = FADE_OUT_MS + FADE_IN_MS + 400;
  int bad = 0;
  for (int sequence = 0; sequence < 400; sequence++) {
    flushing = false;
    const int count = 2 + (int)random(6);
    std::vector<Op> ops;
    std::vector<uint32_t> gaps;
    ops.push_back(EXTRACT);
    gaps.push_back(random(3) == 0 ? random(300) : 600 + random(4000));
    for (int i = 1; i < count; i++) {
      const uint32_t r = random(100);
      ops.push_back(r < 70 ? PUSH : (r < 80 ? MODE : UP));
      const uint32_t g = random(100);
      gaps.push_back(g < 30 ? 0 : (g < 60 ? random(120) : random(1500)));
    }

    ScreenController sc;
    ready(sc);
    for (int i = 0; i < count; i++) {
      apply(sc, ops[i]);
      run(sc, gaps[i]);
    }
    if (flushing) {
      sc.showExtractionProgress(resultMl, 27);   // the flush is over (a blinking tag would show another phase at each look)
    }
    flushing = false;
    // the same live temperature at the end, then everything settles
    run(sc, 1100);
    sc.showLiveTemperature(t, 88.4);
    run(sc, SETTLE);
    const Picture got = snapshot();

    // a complete redraw of the same state (the standby picture, then the screen as it is)
    sc.enterStandby();
    sc.leaveStandby();
    run(sc, SETTLE);
    const Picture whole = snapshot();

    // (the temperature row is not compared: the redraw shows the value of the last reading, not the one of the last display)
    bool same = true;
    for (int y = VOLUME_ZONE_Y - 4; y < 240 && same; y++) {
      for (int x = 0; x < 240; x++) {
        if (got[y * 240 + x] != whole[y * 240 + x]) {
          same = false;
          break;
        }
      }
    }
    if (!same) {
      bad++;
      if (bad <= 3) {
        int minX = 240, minY = 240, maxX = -1, maxY = -1;
        for (int y = VOLUME_ZONE_Y - 4; y < 240; y++) {
          for (int x = 0; x < 240; x++) {
            if (got[y * 240 + x] != whole[y * 240 + x]) {
              if (x < minX) minX = x;
              if (x > maxX) maxX = x;
              if (y < minY) minY = y;
              if (y > maxY) maxY = y;
            }
          }
        }
        printf("sequence %d differs:", sequence);
        for (int i = 0; i < count; i++) {
          printf(" %s +%lu", ops[i] == PUSH ? "push" : (ops[i] == MODE ? "mode" : (ops[i] == UP ? "up" : "extract")), (unsigned long)gaps[i]);
        }
        printf("  (pixels differ in x %d..%d, y %d..%d)\n", minX, maxX, minY, maxY);
      }
    }
  }
  CHECK(bad == 0);
  if (fails == 0) printf("test_fade_machine: ALL OK\n");
  return fails ? 1 : 0;
}
