// The fade of the backlight at a big change of the screen (BACKLIGHT_FADE_OUT_MS, BACKLIGHT_FADE_IN_MS in Config.h): entering and leaving the
// steaming mode and the standby. The order: the backlight goes down while the OLD screen stays untouched, the new screen is drawn only when the
// backlight is off, then the backlight comes up on the new screen. The loop goes on meanwhile (update() every few ms, no delay()). A change that
// comes during a fade only changes the screen drawn in the dark. The real ScreenController and Display.cpp draw into the frame buffer of the GFX
// stub; analogWrite() of the stub calls a hook that looks at the frame buffer at that very moment.
#include <cstdio>
#include <vector>
#define private public
#include "src/Config.h"
#include "src/display/ScreenController.h"
#undef private
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

typedef std::vector<uint16_t> Picture;
static Picture snapshot() { return Picture(frameBuffer, frameBuffer + 240 * 240); }

static uint32_t hashOf() {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 240 * 240; i++) h = (h ^ frameBuffer[i]) * 16777619u;
  return h;
}

// What the hook saw: the clock, the duty cycle written to the backlight and the picture at that moment
struct Event { unsigned long at; int value; uint32_t picture; };
static std::vector<Event> events;
static void hook(uint8_t pin, int value) {
  if (pin == PIN_TFT_BL) events.push_back({t, value, hashOf()});
}

static Settings settings() {
  Settings s;
  s.temperature = 93; s.volume = 40; s.preinfusion = 10; s.preinfusionWait = 5; s.flush = 5; s.manualSeconds = 0; s.flowLimit = 0;
  s.loss = DEFAULT_LOSS_ML; s.manual = false; s.steamingTemperature = 135;
  return s;
}

static const unsigned long STEP = 5;   // a pass of the main loop

// The loop runs for ms, with the live temperature changing (it must not be drawn on a screen that is going dark)
static void run(ScreenController &sc, unsigned long ms) {
  for (unsigned long i = 0; i < ms; i += STEP) {
    t += STEP;
    sc.showLiveTemperature(t, 90.0 + (t / 100) % 7);
    sc.update(t);
  }
}

static void settle(ScreenController &sc) {
  for (int i = 0; i < 400 && sc.animating(); i++) run(sc, STEP);
  CHECK(!sc.animating());
}

static void ready(ScreenController &sc) {
  sc.begin(settings());
  sc.finishWelcome();
  sc.showLiveTemperature(0, 92.4);
  sc.showHeaterState(true);
  t += 2000;
  sc.update(t);
  sc.show();
  settle(sc);
}

// What the backlight did since events were cleared: down to 0 (never up before), dark, then up to 255 (never down). The duty cycle is the
// square of the level, so it is 0 a little before the level is: the dark is the run of 0 values. The new screen (when it is not the same
// picture as the old one, oldHash) is drawn in that run, after its last event of the fade out: before, every picture is the old one, and it is
// the new one in all the events after. Returns the index of the last event of the fade out (the backlight is off).
static size_t checkFade(uint32_t oldHash, bool newScreenDiffers = true) {
  size_t first = 0;
  while (first < events.size() && events[first].value != 0) first++;
  CHECK(first < events.size());
  if (first >= events.size()) return 0;
  size_t last = first;
  while (last + 1 < events.size() && events[last + 1].value == 0) last++;
  for (size_t i = 1; i <= first; i++) CHECK(events[i].value <= events[i - 1].value);
  for (size_t i = last + 1; i < events.size(); i++) CHECK(events[i].value >= events[i - 1].value);
  CHECK(events.back().value == 255);
  if (!newScreenDiffers) return last;

  size_t change = 0;   // the first event that sees the new screen
  while (change < events.size() && events[change].picture == oldHash) change++;
  CHECK(change > 0 && change < events.size());
  if (change == 0 || change >= events.size()) return last;
  CHECK(events[change - 1].value == 0);   // the old screen was dark when the new one was drawn
  for (size_t i = change; i < events.size(); i++) CHECK(events[i].picture != oldHash);
  return change - 1;
}

// A complete redraw of the state: what the screen must show when everything has settled
static Picture completeRedraw(ScreenController &sc) {
  sc.drawNewScreen();
  sc.update(t);
  return snapshot();
}

int main() {
  stubAnalogHook = hook;
  ScreenController sc;
  ready(sc);
  CHECK(events.back().value == 255);      // the start-up fade in ended
  CHECK(sc.backlight_.level == 255);

  // ---- the start-up: the welcome picture is drawn in the dark, then the backlight rises to full in steps (no jump)
  CHECK(events.size() > 8 && events[0].value == 0);
  size_t lit = 1;
  while (lit < events.size() && events[lit].value == 0) lit++;
  CHECK(lit < events.size() && events[lit].picture != events[0].picture);   // the picture was there when the light began
  size_t full = lit;
  while (full < events.size() && events[full].value < 255) full++;
  CHECK(full < events.size() && full - lit >= 10);                           // a fade, not a jump
  for (size_t i = lit + 1; i <= full && i < events.size(); i++) CHECK(events[i].value >= events[i - 1].value);

  // ---- entering the steaming mode
  uint32_t oldHash = hashOf();
  events.clear();
  unsigned long requestAt = t;
  sc.enterSteaming();
  CHECK(sc.animating());
  CHECK(hashOf() == oldHash);              // nothing is drawn by the call itself: the old screen is lit
  settle(sc);
  size_t zero = checkFade(oldHash);
  // durations: about BACKLIGHT_FADE_OUT_MS to be off (the start is taken by the first update, one pass later), then about BACKLIGHT_FADE_IN_MS
  CHECK(events[zero].at - requestAt >= BACKLIGHT_FADE_OUT_MS && events[zero].at - requestAt <= BACKLIGHT_FADE_OUT_MS + 3 * STEP);
  CHECK(events.back().at - events[zero].at >= BACKLIGHT_FADE_IN_MS && events.back().at - events[zero].at <= BACKLIGHT_FADE_IN_MS + 3 * STEP);
  // the curve is perceptual: the duty cycle is the square of the level, so at the middle of the fade out it is a quarter, not a half
  CHECK(events[zero / 2].value < 255 * 3 / 8);
  const Picture steam = snapshot();
  CHECK(sc.steaming());
  CHECK(completeRedraw(sc) == steam);       // the picture is what a complete redraw gives

  // ---- leaving it
  oldHash = hashOf();
  events.clear();
  sc.leaveSteaming();
  CHECK(hashOf() == oldHash);
  settle(sc);
  checkFade(oldHash);
  const Picture brewing = snapshot();
  CHECK(brewing != steam);
  CHECK(!sc.steaming());
  CHECK(completeRedraw(sc) == brewing);

  // ---- a second change during the fade out: only the screen drawn in the dark changes; the backlight does not go up again
  oldHash = hashOf();
  events.clear();
  sc.enterSteaming();
  run(sc, 60);
  sc.leaveSteaming();                       // back before the old screen went dark
  settle(sc);
  checkFade(oldHash, false);                // (the screen drawn is the brewing one, as the one that went dark)
  CHECK(!sc.steaming());
  CHECK(snapshot() != steam);
  CHECK(snapshot() == completeRedraw(sc));   // the temperature moved meanwhile: compared with a redraw of the state

  // ---- a second change during the fade in: the backlight goes down again from where it is, the screen shown is not touched until it is off
  events.clear();
  sc.enterSteaming();
  run(sc, BACKLIGHT_FADE_OUT_MS + 60);      // dark, drawn, rising
  CHECK(sc.backlight_.phase == ScreenController::BACKLIGHT_IN);
  const int level = sc.backlight_.level;
  CHECK(level > 0 && level < 255);
  const uint32_t steamHash = hashOf();
  CHECK(steamHash != oldHash);
  events.clear();
  sc.leaveSteaming();
  settle(sc);
  CHECK(events[0].value <= level * level / 255);   // it starts down from where it was
  checkFade(steamHash);
  CHECK(!sc.steaming() && snapshot() != steam && snapshot() == completeRedraw(sc));

  // ---- the standby: the picture comes in the dark, nothing else is drawn on it, the wake-up draws the real screen in the dark
  oldHash = hashOf();
  events.clear();
  sc.enterStandby();
  CHECK(sc.animating());
  settle(sc);
  checkFade(oldHash);
  const Picture picture = snapshot();
  CHECK(picture != brewing);
  run(sc, 3000);                            // the temperature changes: nothing is drawn over the picture
  CHECK(snapshot() == picture);
  oldHash = hashOf();
  events.clear();
  sc.leaveStandby();
  settle(sc);
  checkFade(oldHash);
  CHECK(snapshot() != picture);
  CHECK(completeRedraw(sc) == snapshot());

  // ---- standby entered then left before the picture was drawn: the real screen comes back, nothing is left of a picture
  oldHash = hashOf();
  const Picture before = snapshot();
  events.clear();
  sc.enterStandby();
  run(sc, 40);
  sc.leaveStandby();
  settle(sc);
  checkFade(oldHash, false);
  CHECK(sc.backlight_.level == 255 && !sc.standbyShown_);
  CHECK(snapshot() == completeRedraw(sc));
  CHECK(snapshot().size() == before.size());

  printf(fails ? "%d FAIL\n" : "test_backlight: ALL OK\n", fails);
  return fails != 0;
}
