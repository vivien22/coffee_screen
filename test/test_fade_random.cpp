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

static bool blinking = false;   // the last phase shown is FLUSHING (its tag blinks)
static void ready(ScreenController &sc) {
  blinking = false;
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

// The phases of an extraction as the machine shows them: PRE, WAIT, the main extraction, FLUSH waiting for the push or counting down, the
// result over (ready). Their tags appear and disappear with a fade.
enum Op { PUSH, MODE, UP, PRE, WAIT, MAIN, FLUSH_WAIT, FLUSHING, READY };
static int opNumber = 0;
static uint16_t flushMl = 0, flushLeft = 0;

static void apply(ScreenController &sc, Op op) {
  if (op == PUSH) {
    sc.selectNextParameter();
  } else if (op == MODE) {
    sc.toggleMode();
    sc.showReady();   // what CoffeeMachine does just after
  } else if (op == UP) {
    sc.adjustActiveParameter(1);
  } else {
    const int n = ++opNumber;   // the numbers shown depend on the operation only: both runs of a sequence show the same ones
    blinking = false;
    if (op == PRE) sc.showPreinfusion(10 - n % 9, n % 20);
    else if (op == WAIT) sc.showPreinfusionWait(10 + n % 9, 1 + n % 5);
    else if (op == MAIN) sc.showExtractionProgress(20 + n % 40, 10 + n % 20);
    else if (op == FLUSH_WAIT) sc.showFlush(30 + n % 30, 20 + n % 9, false);
    else if (op == FLUSHING) { flushMl = 30 + n % 30; flushLeft = 1 + n % 9; blinking = true; sc.showFlush(flushMl, flushLeft, true); }
    else sc.showReady();
  }
}

// The blinking tag of FLUSHING is shown or hidden according to the time: the end of a sequence that left it blinking shows it fixed (the same
// tag, no blinking; other operations of the sequence do not stop it), as the machine does when the flush waits for the push, so that both runs of a sequence can be compared
static void stopBlinking(ScreenController &sc) {
  if (blinking) {
    sc.showFlush(flushMl, flushLeft, false);
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
  for (int sequence = 0; sequence < 1500; sequence++) {
    const int count = 1 + (int)random(7);
    std::vector<Op> ops;
    std::vector<uint32_t> gaps;
    for (int i = 0; i < count; i++) {
      const uint32_t r = random(100);
      ops.push_back(r < 40 ? PUSH : (r < 50 ? MODE : (r < 55 ? UP : (r < 65 ? PRE : (r < 72 ? WAIT : (r < 79 ? MAIN : (r < 86 ? FLUSH_WAIT : (r < 93 ? FLUSHING : READY))))))));
      const uint32_t g = random(100);
      gaps.push_back(g < 30 ? 0 : (g < 60 ? random(120) : random(1500)));
    }

    Picture got, expected;
    {
      ScreenController sc;
      ready(sc);
      for (int i = 0; i < count; i++) {
        opNumber = i * 7;
        apply(sc, ops[i]);
        run(sc, gaps[i]);
      }
      stopBlinking(sc);
      settleTemperature(sc);
      got = snapshot();
    }
    {
      ScreenController sc;
      ready(sc);
      for (int i = 0; i < count; i++) {
        opNumber = i * 7;
        apply(sc, ops[i]);
        run(sc, SETTLE);
      }
      stopBlinking(sc);
      settleTemperature(sc);
      expected = snapshot();
    }
    if (got != expected) {
      bad++;
      if (bad <= 3) {
        printf("sequence %d differs:", sequence);
        for (int i = 0; i < count; i++) printf(" %d +%lu", (int)ops[i], (unsigned long)gaps[i]);
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
