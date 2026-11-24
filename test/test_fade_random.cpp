// Random pushes (next parameter, change of mode, a value set) at random moments, during the fades of the previous ones: once everything has
// settled, the screen must be the one that the same pushes give when each one is made on a quiet screen (the fades are over, nothing is left of
// the looks that were interrupted). The real ScreenController and the real Display.cpp draw into the frame buffer of the GFX stub.
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

// The live temperature changes every second, as on the machine (the field of the temperature is drawn again meanwhile)
static bool liveVaries = true;
static void run(ScreenController &sc, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    t++;
    if (liveVaries && t % 1000 == 0) {
      sc.showLiveTemperature(t, 88.0 + (t / 1000 % 5) * 0.4);
    }
    sc.update(t);
  }
}

// The end of a sequence: the same live temperature in both screens (it is shown at most once a second), then everything settles
static void settleTemperature(ScreenController &sc) {
  liveVaries = false;
  run(sc, 1100);
  sc.showLiveTemperature(t, 88.4);
  run(sc, FADE_OUT_MS + FADE_IN_MS + 400);
  liveVaries = true;
}

enum Op { PUSH, MODE, UP };

static void apply(ScreenController &sc, Op op) {
  if (op == PUSH) {
    sc.selectNextParameter();
  } else if (op == MODE) {
    sc.toggleMode();
    sc.showReady();   // what CoffeeMachine does just after
  } else {
    sc.adjustActiveParameter(1);
  }
}

static uint32_t seed = 12345;
static uint32_t random(uint32_t n) {
  seed = seed * 1664525u + 1013904223u;
  return (seed >> 8) % n;
}

int main() {
  const uint32_t SETTLE = FADE_OUT_MS + FADE_IN_MS + 400;
  int bad = 0;
  for (int sequence = 0; sequence < 600; sequence++) {
    const int count = 1 + (int)random(7);
    std::vector<Op> ops;
    std::vector<uint32_t> gaps;
    for (int i = 0; i < count; i++) {
      const uint32_t r = random(100);
      ops.push_back(r < 70 ? PUSH : (r < 85 ? MODE : UP));
      const uint32_t g = random(100);
      gaps.push_back(g < 30 ? 0 : (g < 60 ? random(120) : random(1500)));
    }

    Picture got, expected;
    {
      ScreenController sc;
      ready(sc);
      for (int i = 0; i < count; i++) {
        apply(sc, ops[i]);
        run(sc, gaps[i]);
      }
      settleTemperature(sc);
      got = snapshot();
    }
    {
      ScreenController sc;
      ready(sc);
      for (int i = 0; i < count; i++) {
        apply(sc, ops[i]);
        run(sc, SETTLE);
      }
      settleTemperature(sc);
      expected = snapshot();
    }
    if (got != expected) {
      bad++;
      if (bad <= 3) {
        printf("sequence %d differs:", sequence);
        for (int i = 0; i < count; i++) printf(" %s +%lu", ops[i] == PUSH ? "push" : (ops[i] == MODE ? "mode" : "up"), (unsigned long)gaps[i]);
        int minX = 240, minY = 240, maxX = -1, maxY = -1;
        for (int y = 0; y < 240; y++) {
          for (int x = 0; x < 240; x++) {
            if (got[y * 240 + x] != expected[y * 240 + x]) {
              if (x < minX) minX = x;
              if (x > maxX) maxX = x;
              if (y < minY) minY = y;
              if (y > maxY) maxY = y;
            }
          }
        }
        printf("  (pixels differ in x %d..%d, y %d..%d)\n", minX, maxX, minY, maxY);
      }
    }
  }
  CHECK(bad == 0);
  if (fails == 0) printf("test_fade_random: ALL OK\n");
  return fails ? 1 : 0;
}
