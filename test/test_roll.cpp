// The rolling digits of the volume and of the duration during an extraction (ROLL_VOLUME_MS, ROLL_DURATION_MS in Config.h).
// The real ScreenController and the real Display.cpp draw into the frame buffer of the GFX stub; the clock is the one of the stubs.
// Checked: the end of a roll is the picture of the number drawn as it is, a roll has its frames (about one every ROLL_FRAME_MS), the digits
// that do not change do not move, a new number during a roll ends the roll in progress, and a setting does not roll.
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

static Settings settings(uint8_t volume = 40) {
  Settings s;
  s.temperature = 93;
  s.volume = volume;
  s.preinfusion = 10;
  s.preinfusionWait = 5;
  s.flush = 5;
  s.manualSeconds = 0;
  s.flowLimit = 0;
  s.loss = DEFAULT_LOSS_ML;
  s.manual = false;
  return s;
}

static void ready(ScreenController &sc, uint8_t volume = 40) {
  sc.begin(settings(volume));
  sc.finishWelcome();
  sc.showLiveTemperature(0, 92.4);
  sc.showHeaterState(true);
  t += 2000;
  sc.update(t);
}

// Runs the screen for ms milliseconds, one update() per millisecond. Returns how many of them changed the picture.
static int run(ScreenController &sc, uint32_t ms) {
  int changes = 0;
  Picture before = snapshot();
  for (uint32_t i = 0; i < ms; i++) {
    t++;
    sc.update(t);
    Picture now = snapshot();
    if (now != before) {
      changes++;
      before = now;
    }
  }
  return changes;
}

// The picture of an extraction showing (volume, seconds) drawn as it is: the two fields are drawn again with progress =
// DISPLAY_ROLL_DONE and redrawTag (the whole field is erased and drawn, whatever was there), over a screen that has been through the
// extraction. The reference is made after the picture it is compared with: both use the frame buffer.
static Picture asItIs(uint16_t volume, uint16_t seconds) {
  ScreenController sc;
  ready(sc);
  sc.showExtractionStarted();
  sc.showExtractionProgress(volume, seconds);
  run(sc, 1000);
  drawVolume(volume, TEXT_SIZE_LARGE, PHASE_TAG_NONE, true);
  drawDuration(seconds, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
  return snapshot();
}

// Does the rectangle show the same pixels in the two pictures?
static bool sameIn(const Picture &a, const Picture &b, int x0, int y0, int x1, int y1) {
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      if (a[y * 240 + x] != b[y * 240 + x]) return false;
    }
  }
  return true;
}

// Where two pictures differ (printed when a check fails)
static void whereDiffer(const Picture &a, const Picture &b) {
  int x0 = 240, y0 = 240, x1 = -1, y1 = -1;
  for (int y = 0; y < 240; y++) for (int x = 0; x < 240; x++) if (a[y * 240 + x] != b[y * 240 + x]) {
    if (x < x0) x0 = x; if (y < y0) y0 = y; if (x > x1) x1 = x; if (y > y1) y1 = y;
  }
  printf("  differ in x %d..%d, y %d..%d\n", x0, x1, y0, y1);
}

static void startedAt(ScreenController &sc, uint16_t volume, uint16_t seconds) {
  ready(sc);
  sc.showExtractionStarted();
  sc.showExtractionProgress(volume, seconds);
  run(sc, 1000);   // the first picture is complete
}

int main() {
  // 1. The end of a roll is the number drawn as it is, for the volume and the duration
  {
    ScreenController sc;
    startedAt(sc, 25, 18);
    sc.showExtractionProgress(26, 19);
    run(sc, 1000);
    { const Picture got = snapshot(); const Picture ref = asItIs(26, 19); CHECK(got == ref); if (got != ref) whereDiffer(got, ref); }   // the reference is drawn after: it uses the same frame buffer
  }

  // 2. A roll (of the tens: 29 -> 30) has frames, about one every ROLL_FRAME_MS, and they are neither the old picture nor the new one
  {
    ScreenController sc;
    startedAt(sc, 29, 18);
    const Picture before = snapshot();
    sc.showExtractionProgress(30, 18);
    t++;
    sc.update(t);
    CHECK(sameIn(snapshot(), before, VOLUME_ZONE_X, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT));   // the old number until the first frame
    int changes = run(sc, 40);   // the first frame comes ROLL_FRAME_MS after the start
    Picture frame = snapshot();
    CHECK(!sameIn(frame, before, VOLUME_ZONE_X, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT));
    changes += run(sc, ROLL_VOLUME_MS);
    printf("roll of the volume: %d pictures in %lu ms\n", changes, (unsigned long)ROLL_VOLUME_MS);
    CHECK(changes >= 3 && changes <= (int)(ROLL_VOLUME_MS / ROLL_FRAME_MS) + 3);
    const Picture last = snapshot();
    const Picture after = asItIs(30, 18);
    CHECK(last == after);
    CHECK(!sameIn(frame, after, VOLUME_ZONE_X, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT));
  }

  // 3. The volume rolls on its tens only: a change of the ones (25 -> 26) is on the screen at the first update(), as drawn as it is, and the
  // tens do not move
  {
    ScreenController sc;
    startedAt(sc, 25, 18);
    const Picture before = snapshot();
    sc.showExtractionProgress(26, 18);
    t++;
    sc.update(t);
    { const Picture got = snapshot(); const Picture ref = asItIs(26, 18); CHECK(got == ref); if (got != ref) whereDiffer(got, ref); }
    sc.showExtractionProgress(27, 18);
    t++;
    sc.update(t);
    bool tensStill = true;
    for (uint32_t i = 0; i < ROLL_VOLUME_MS + 40; i++) {
      t++;
      sc.update(t);
      tensStill = tensStill && sameIn(snapshot(), before, VOLUME_X, VOLUME_ZONE_Y, VOLUME_X + DIGIT_CELL_LARGE, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT);
    }
    CHECK(tensStill);
  }

  // 4. A number that gains a digit (9 -> 10) rolls too, and ends as drawn as it is
  {
    ScreenController sc;
    startedAt(sc, 9, 18);
    sc.showExtractionProgress(10, 18);
    run(sc, 1000);
    { const Picture got = snapshot(); CHECK(got == asItIs(10, 18)); }   // the reference is drawn after: it uses the same frame buffer
  }

  // 5. A new number during a roll ends the roll in progress, then the new one rolls
  {
    ScreenController sc;
    startedAt(sc, 28, 18);
    sc.showExtractionProgress(29, 18);
    run(sc, 60);
    sc.showExtractionProgress(30, 18);   // the tens roll
    run(sc, 20);
    sc.showExtractionProgress(31, 18);
    run(sc, 1000);
    { const Picture got = snapshot(); const Picture ref = asItIs(31, 18); CHECK(got == ref); if (got != ref) whereDiffer(got, ref); }   // the reference is drawn after: it uses the same frame buffer
  }

  // 6. The duration rolls in ROLL_DURATION_MS, the volume is not touched
  {
    ScreenController sc;
    startedAt(sc, 25, 18);
    const Picture before = snapshot();
    sc.showExtractionProgress(25, 19);
    int changes = run(sc, 1);
    bool volumeStill = true;
    for (uint32_t i = 0; i < ROLL_DURATION_MS + 40; i++) {
      t++;
      sc.update(t);
      volumeStill = volumeStill && sameIn(snapshot(), before, VOLUME_ZONE_X, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT);
    }
    CHECK(volumeStill);
    { const Picture got = snapshot(); const Picture ref = asItIs(25, 19); CHECK(got == ref); if (got != ref) whereDiffer(got, ref); }   // the reference is drawn after: it uses the same frame buffer
    CHECK(changes <= 1);
  }

  // 7. A setting does not roll: the new value is on the screen at the first update(), as drawn as it is
  {
    ScreenController sc;
    ready(sc);
    sc.adjustActiveParameter(1);
    t++;
    sc.update(t);
    const Picture shown = snapshot();
    ScreenController other;
    ready(other, 41);
    CHECK(sameIn(shown, snapshot(), VOLUME_ZONE_X, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT));
  }

  if (fails == 0) printf("test_roll: ALL OK\n");
  return fails ? 1 : 0;
}
