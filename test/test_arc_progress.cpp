// The progress arc of the screen (ScreenController::showWaterSent): it follows the water that went through the machine, and it does not stop
// during the wait of the pre-infusion, when no water flows: the seconds of the wait count as water at ARC_WAIT_ML_PER_S. The whole circle is
// the target, the loss and the wait. The real ScreenController and Display.cpp draw into the frame buffer of the GFX stub.
#include <cstdio>
#include <cmath>
#define private public
#include "src/Config.h"
#include "src/display/ScreenController.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];
static unsigned long &now_ = stubNow;

static Settings settings(uint8_t preinfusion, uint8_t wait) {
  Settings s;
  s.temperature = 93;
  s.volume = 40;
  s.preinfusion = preinfusion;
  s.preinfusionWait = wait;
  s.flush = 5;
  s.manualSeconds = 0;
  s.flowLimit = 0;
  s.loss = 50.0f;
  s.manual = false;
  return s;
}

// The pixels of the ring that are not the background
static int ringPixels() {
  int n = 0;
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      const double r = hypot(x + 0.5 - 120.0, y + 0.5 - 120.0);
      if (r > ARC_OUTER_RADIUS - ARC_WIDTH - 0.5 && frameBuffer[y * 240 + x] != COLOR_BG) n++;
    }
  }
  return n;
}

static void settle(ScreenController &sc) {
  for (int i = 0; i < 100; i++) {   // 12 segments per update
    now_ += 40;
    sc.update(now_);
  }
}

int main() {
  // a pre-infusion of 40 ml and a wait of 5 s: the circle is 40 + 50 + 5 x ARC_WAIT_ML_PER_S ml
  ScreenController sc;
  sc.begin(settings(40, 5));
  sc.finishWelcome();
  now_ += 2000;
  settle(sc);
  CHECK(ringPixels() == 0);                          // nothing in the ready state
  sc.showExtractionStarted();
  sc.showWaterSent(40.0f, 0.0f);                     // the end of the pre-infusion: the pump stops, the wait begins
  settle(sc);
  const int atWaitStart = ringPixels();
  CHECK(atWaitStart > 0);
  sc.showWaterSent(40.0f, 2.5f);                     // the same water, 2.5 s of wait later: the arc has moved
  settle(sc);
  const int atWaitMiddle = ringPixels();
  CHECK(atWaitMiddle > atWaitStart);
  sc.showWaterSent(40.0f, 5.0f);                     // the end of the wait
  settle(sc);
  const int atWaitEnd = ringPixels();
  CHECK(atWaitEnd > atWaitMiddle);
  // the extraction goes on from there: no jump back, and the water brings the arc to the full circle at the target (net = target + loss)
  sc.showWaterSent(60.0f, 5.0f);
  settle(sc);
  const int atMiddle = ringPixels();
  CHECK(atMiddle > atWaitEnd);
  sc.showWaterSent(90.0f, 5.0f);
  settle(sc);
  const int full = ringPixels();
  CHECK(full > atMiddle);
  sc.showWaterSent(200.0f, 5.0f);                    // more water than needed: the circle stays full
  settle(sc);
  CHECK(ringPixels() == full);
  // the circle is full only when the wait and the water are all there: without the wait it is not
  sc.showReady();
  settle(sc);
  CHECK(ringPixels() == 0);
  sc.showExtractionStarted();
  sc.showWaterSent(90.0f, 0.0f);
  settle(sc);
  CHECK(ringPixels() < full);
  sc.showWaterSent(90.0f, 5.0f);
  settle(sc);
  CHECK(ringPixels() == full);

  // no pre-infusion: no wait, the wait seconds count for nothing and the circle is the target and the loss
  ScreenController plain;
  plain.begin(settings(0, 5));
  plain.finishWelcome();
  now_ += 2000;
  settle(plain);
  plain.showExtractionStarted();
  plain.showWaterSent(90.0f, 5.0f);
  settle(plain);
  CHECK(ringPixels() == full);
  plain.showWaterSent(45.0f, 5.0f);                  // half the water: not full (the wait seconds are not added)
  settle(plain);
  CHECK(ringPixels() < full && ringPixels() > 0);

  // a change of the screen (standby): the ring is not drawn on the old screen while it fades out, and it is drawn again, in full, with the
  // new screen in the dark
  ScreenController change;
  change.begin(settings(0, 5));
  change.finishWelcome();
  now_ += 2000;
  settle(change);
  change.showExtractionStarted();
  change.showWaterSent(45.0f, 0.0f);
  settle(change);
  const int before = ringPixels();
  CHECK(before > 0 && before < full);
  change.enterStandby();
  change.showWaterSent(90.0f, 0.0f);                 // more water while the old screen fades out
  int outFrames = 0;
  while (change.backlight_.phase() == BacklightFade::OUT) {
    now_ += 10;
    change.update(now_);
    outFrames++;
    CHECK(outFrames < 1000);
    if (change.backlight_.phase() == BacklightFade::OUT) CHECK(ringPixels() == before);   // untouched until it is dark
  }
  change.leaveStandby();
  for (int i = 0; i < 400; i++) {
    now_ += 10;
    change.update(now_);
  }
  CHECK(change.backlight_.phase() == BacklightFade::STEADY);
  CHECK(ringPixels() == full);                       // the ring was redrawn with the screen, at the water of that moment

  if (fails == 0) printf("test_arc_progress: ALL OK\n");
  return fails ? 1 : 0;
}
