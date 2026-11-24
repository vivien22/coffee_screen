// The change of parameter (FADE_OUT_MS, FADE_IN_MS in Config.h): the row whose look changes fades out (its text goes dark), then the new row
// fades in; the rows that do not change are not touched; the old and the new rows are never drawn together; a change during a fade shows the
// final look at once and starts the next one; a value set on a row that fades ends the fade. The real ScreenController and the real
// Display.cpp draw into the frame buffer of the GFX stub; the clock is the one of the stubs.
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

// The three rows, as rectangles of the screen (the zones of Layout.h)
struct Zone { int x, y, w, h; };
static const Zone TEMPERATURE_ROW = {TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT};
static const Zone VOLUME_ROW = {VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT};
static const Zone DURATION_ROW = {DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT};

static bool sameIn(const Picture &a, const Picture &b, const Zone &z) {
  for (int y = z.y; y < z.y + z.h; y++) {
    for (int x = z.x; x < z.x + z.w; x++) {
      if (a[y * 240 + x] != b[y * 240 + x]) return false;
    }
  }
  return true;
}

// The pixels of the zone that are not the background
static int inkIn(const Picture &p, const Zone &z) {
  int n = 0;
  for (int y = z.y; y < z.y + z.h; y++) {
    for (int x = z.x; x < z.x + z.w; x++) n += p[y * 240 + x] != COLOR_BG;
  }
  return n;
}

// The brightest pixel of the zone (the largest sum of the 5 + 6 + 5 bits of its channels, a measure of its brightness)
static int brightestIn(const Picture &p, const Zone &z) {
  int best = 0;
  for (int y = z.y; y < z.y + z.h; y++) {
    for (int x = z.x; x < z.x + z.w; x++) {
      const uint16_t c = p[y * 240 + x];
      const int sum = ((c >> 11) & 31) * 2 + ((c >> 5) & 63) + (c & 31) * 2;
      if (sum > best) best = sum;
    }
  }
  return best;
}

// Is every pixel of the zone that is not the background also not the background in the mask picture? (the ink is inside the mask's ink)
static bool inkInside(const Picture &p, const Picture &mask, const Zone &z) {
  for (int y = z.y; y < z.y + z.h; y++) {
    for (int x = z.x; x < z.x + z.w; x++) {
      if (p[y * 240 + x] != COLOR_BG && mask[y * 240 + x] == COLOR_BG) return false;
    }
  }
  return true;
}

static void run(ScreenController &sc, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    t++;
    sc.update(t);
  }
}

int main() {
  const uint32_t TOTAL = FADE_OUT_MS + FADE_IN_MS;

  // The pictures of the volume row before and after the change volume -> pre-infusion. Each one is made by its own screen, and the one
  // that is checked is made again afterwards (every screen draws into the same frame buffer).
  Picture oldPicture, newPicture;
  {
    ScreenController sc;
    ready(sc);
    oldPicture = snapshot();
  }
  {
    ScreenController sc;
    ready(sc);
    sc.selectNextParameter();
    run(sc, TOTAL + 200);
    newPicture = snapshot();
  }

  // 1. volume -> pre-infusion: the volume row fades out then in, the other rows are not touched, the end is the row as it is
  {
    ScreenController sc;
    ready(sc);
    const Picture start = snapshot();
    sc.selectNextParameter();
    t++;
    sc.update(t);   // the clock of the fade starts here
    const uint32_t begin = t;

    bool othersStill = true, inkInOld = true, inkInNew = true, dark = false, sawNew = false;
    int lastBrightness = 1000, firstChange = -1, outFrames = 0, inFrames = 0, lastIn = 0, previousIn = 0;
    bool darkSeen = false, monotoneOut = true, monotoneIn = true;
    uint32_t darkAt = 0;
    Picture previous = start;
    for (uint32_t i = 0; i < TOTAL + 100; i++) {
      t++;
      sc.update(t);
      const Picture now = snapshot();
      othersStill = othersStill && sameIn(now, start, TEMPERATURE_ROW) && sameIn(now, start, DURATION_ROW);
      if (!sameIn(now, previous, VOLUME_ROW)) {
        if (firstChange < 0) firstChange = t - begin;
        const int ink = inkIn(now, VOLUME_ROW);
        const int bright = brightestIn(now, VOLUME_ROW);
        if (ink == 0) {
          darkSeen = true;
          darkAt = t - begin;
        } else if (!darkSeen) {
          outFrames++;
          inkInOld = inkInOld && inkInside(now, oldPicture, VOLUME_ROW);   // the old look: the digits "40" and "ml"
          monotoneOut = monotoneOut && bright <= lastBrightness;
          lastBrightness = bright;
        } else {
          inFrames++;
          inkInNew = inkInNew && inkInside(now, newPicture, VOLUME_ROW);   // the new look: "10", "ml" and the tag PRE
          monotoneIn = monotoneIn && bright >= previousIn;
          previousIn = bright;
          lastIn = bright;
        }
      }
      previous = now;
    }
    (void)dark; (void)sawNew;
    printf("fade of the volume row: first picture at %d ms, %d frames out, dark at %lu ms, %d frames in\n", firstChange, outFrames,
           (unsigned long)darkAt, inFrames);
    CHECK(othersStill);
    CHECK(firstChange >= (int)FADE_FRAME_MS - 2 && firstChange <= (int)FADE_FRAME_MS + 2);
    CHECK(outFrames >= 3);
    CHECK(darkSeen);
    CHECK(darkAt + 5 >= FADE_OUT_MS - 2 * FADE_FRAME_MS && darkAt <= FADE_OUT_MS + 2 * FADE_FRAME_MS);   // about at the end of the fade out
    CHECK(inFrames >= 8);
    CHECK(inkInOld);    // the old row is never mixed with the new one
    CHECK(inkInNew);
    CHECK(monotoneOut);
    CHECK(monotoneIn);
    CHECK(lastIn > 0);
    const Picture finished = snapshot();
    CHECK(sameIn(finished, newPicture, VOLUME_ROW) && sameIn(finished, newPicture, TEMPERATURE_ROW) && sameIn(finished, newPicture, DURATION_ROW));
  }

  // 2. A new change during a fade: the final look of the first one is on the screen at once, then the next fade starts from it
  {
    ScreenController sc;
    ready(sc);
    sc.selectNextParameter();       // volume -> pre-infusion
    run(sc, 120);
    sc.selectNextParameter();       // pre-infusion -> flow limit, called by the machine: nothing is drawn by update() yet
    const Picture shown = snapshot();
    CHECK(sameIn(shown, newPicture, VOLUME_ROW));    // the pre-infusion as it is, at full brightness
    run(sc, TOTAL + 200);
    // the flow limit as it is (the volume row of the third parameter), made after: it uses the same frame buffer
    const Picture got = snapshot();
    ScreenController other;
    ready(other);
    other.selectNextParameter();
    run(other, TOTAL + 100);
    other.selectNextParameter();
    run(other, TOTAL + 200);
    const Picture expected = snapshot();
    CHECK(sameIn(got, expected, VOLUME_ROW) && sameIn(got, expected, TEMPERATURE_ROW) && sameIn(got, expected, DURATION_ROW));
  }

  // 3. A value set on a row that fades ends the fade at once and is drawn as it is
  {
    ScreenController sc;
    ready(sc);
    sc.selectNextParameter();
    run(sc, 100);
    sc.adjustActiveParameter(1);    // pre-infusion 10 -> 11
    t++;
    sc.update(t);
    const Picture got = snapshot();
    ScreenController other;
    ready(other);
    other.selectNextParameter();
    run(other, TOTAL + 100);
    other.adjustActiveParameter(1);
    t++;
    other.update(t);
    const Picture expected = snapshot();
    CHECK(sameIn(got, expected, VOLUME_ROW));
  }

  // 4. Up to the temperature (5 pushes): the temperature row and the duration row change their look, the picture ends as it is
  {
    ScreenController sc;
    ready(sc);
    for (int i = 0; i < 5; i++) {
      sc.selectNextParameter();
      run(sc, TOTAL + 100);
    }
    const Picture got = snapshot();
    // the same fields drawn again as they are (the whole rows): nothing changes on the screen
    drawTemperature(93.0, TEXT_SIZE_LARGE);
    drawVolume(40, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
    drawDuration(0, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
    const Picture asItIs = snapshot();
    CHECK(sameIn(got, asItIs, TEMPERATURE_ROW) && sameIn(got, asItIs, VOLUME_ROW) && sameIn(got, asItIs, DURATION_ROW));
    CHECK(inkIn(got, TEMPERATURE_ROW) > 0);
  }

  // 5. A change of mode (AUTO -> MANUAL): the drop (its icon and its letter) and the rows whose content changes (the volume 40 -> 0) fade as at a
  // change of parameter; the temperature is not touched; the end is the screen of the new mode drawn as it is
  {
    const Zone DROP = {ICON_DROP_X, ICON_DROP_Y, ICON_DROP_WIDTH, ICON_DROP_HEIGHT};
    Picture manualScreen, autoScreen, blank;
    {
      beginDisplay();   // the screen filled with the background: the drop at brightness 0
      blank = snapshot();
    }
    {
      ScreenController sc;
      Settings s = settings();
      s.manual = true;
      sc.begin(s);
      sc.finishWelcome();
      sc.showLiveTemperature(0, 92.4);
      sc.showHeaterState(true);
      t += 2000;
      sc.update(t);
      manualScreen = snapshot();
    }
    ScreenController sc;
    ready(sc);
    autoScreen = snapshot();
    sc.toggleMode();
    sc.showReady();   // what CoffeeMachine does just after
    t++;
    sc.update(t);     // the clock of the fade starts here
    bool dropWasDark = false;
    bool temperatureStill = true;
    for (uint32_t i = 0; i < TOTAL + 100; i++) {
      t++;
      sc.update(t);
      const Picture now = snapshot();
      temperatureStill = temperatureStill && sameIn(now, autoScreen, TEMPERATURE_ROW);
      dropWasDark = dropWasDark || sameIn(now, blank, DROP);
    }
    const Picture got = snapshot();
    CHECK(temperatureStill);
    CHECK(dropWasDark);   // the drop went dark on the way
    CHECK(sameIn(got, manualScreen, VOLUME_ROW) && sameIn(got, manualScreen, DURATION_ROW) && sameIn(got, manualScreen, TEMPERATURE_ROW));
    CHECK(sameIn(got, manualScreen, DROP));   // the drop with the M, at full brightness
  }

  // 6. The row of the parameter that was selected (here the volume row, LIMIT -> WAIT) changes at once, without a fade, while the row of the new
  // parameter (the duration row) fades out in FADE_OUT_NEW_ROW_MS and in in FADE_IN_NEW_ROW_MS
  {
    Picture finalPicture;
    {
      ScreenController sc;
      ready(sc);
      for (int i = 0; i < 3; i++) {
        sc.selectNextParameter();
        run(sc, TOTAL + 100);
      }
      finalPicture = snapshot();
    }
    ScreenController sc;
    ready(sc);
    for (int i = 0; i < 2; i++) {
      sc.selectNextParameter();
      run(sc, TOTAL + 100);
    }
    sc.selectNextParameter();   // LIMIT -> WAIT
    t++;
    sc.update(t);               // the clock of the fade starts here
    run(sc, 60);
    const Picture early = snapshot();
    CHECK(sameIn(early, finalPicture, VOLUME_ROW));      // the old parameter is back at once
    CHECK(!sameIn(early, finalPicture, DURATION_ROW));   // the new one is still coming up
    // The new row has the shorter fades of a change of row: 330 ms after the start it is already well lit (with the fades of a sub-parameter,
    // FADE_OUT_MS and FADE_IN_MS, it would be at a third of its brightness)
    run(sc, 330 - 60);
    CHECK(brightestIn(snapshot(), DURATION_ROW) * 10 >= brightestIn(finalPicture, DURATION_ROW) * 6);
    run(sc, TOTAL + 100);
    const Picture late = snapshot();
    CHECK(sameIn(late, finalPicture, VOLUME_ROW) && sameIn(late, finalPicture, DURATION_ROW));
  }

  if (fails == 0) printf("test_fade: ALL OK\n");
  return fails ? 1 : 0;
}
