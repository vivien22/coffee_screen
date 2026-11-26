// The heater dot in the steaming mode: next to the thermometer, as in the other modes, it follows the heater (it blinks with it) and it
// stays whole when the steaming temperature is drawn again (the zone of that temperature reaches the edge of the dot). The real
// ScreenController and Display.cpp draw into the frame buffer of the GFX stub.
#include <cstdio>
#include <vector>
#include "src/Config.h"
#include "src/display/ScreenController.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

static Settings settings() {
  Settings s;
  s.temperature = 93; s.volume = 40; s.preinfusion = 10; s.preinfusionWait = 5; s.flush = 5; s.manualSeconds = 0; s.flowLimit = 0;
  s.loss = DEFAULT_LOSS_ML; s.manual = false; s.steamingTemperature = 135;
  return s;
}

static void run(ScreenController &sc, int passes) {
  for (int i = 0; i < passes; i++) {
    t += 10;
    sc.update(t);
  }
}

static void settle(ScreenController &sc) {
  for (int i = 0; i < 300 && sc.animating(); i++) run(sc, 1);
  run(sc, 5);
}

typedef std::vector<uint16_t> Picture;
static Picture snapshot() { return Picture(frameBuffer, frameBuffer + 240 * 240); }

// The brightest pixel of the steaming temperature (its zone): the text at full brightness is COLOR_TEXT; the green channel tells how bright
static const int COLOR_TEXT_LEVEL = (COLOR_TEXT >> 5) & 0x3F;
static int brightestIn() {
  int best = 0;
  for (int y = STEAM_ZONE_Y; y < STEAM_ZONE_Y + STEAM_ZONE_HEIGHT; y++)
    for (int x = STEAM_ZONE_X + 2; x < STEAM_ZONE_X + STEAM_ZONE_WIDTH; x++) {
      const int g = (frameBuffer[y * 240 + x] >> 5) & 0x3F;
      if (g > best) best = g;
    }
  return best;
}

// The screen of the same state drawn whole: the standby picture, then the screen again (the picture of the screen is put back after)
static Picture completeRedraw(ScreenController &sc) {
  const Picture before = snapshot();
  sc.enterStandby();
  sc.leaveStandby();
  for (int i = 0; i < 300 && sc.animating(); i++) run(sc, 1);
  run(sc, 3);
  const Picture whole = snapshot();
  for (int i = 0; i < 240 * 240; i++) frameBuffer[i] = before[i];
  return whole;
}

// The ink of the dot: the pixels of its 9 x 9 box that are not the background
static int dotInk(int cx, int cy) {
  int n = 0;
  for (int y = cy - 4; y <= cy + 4; y++)
    for (int x = cx - 4; x <= cx + 4; x++)
      if (frameBuffer[y * 240 + x] != COLOR_BG) n++;
  return n;
}

int main() {
  ScreenController sc;
  sc.begin(settings());
  sc.finishWelcome();
  sc.showLiveTemperature(t, 92.4);
  sc.showHeaterState(false);
  run(sc, 3);
  sc.show();
  settle(sc);

  sc.enterSteaming();
  settle(sc);
  CHECK(sc.steaming());
  const int off = dotInk(STEAM_DOT_X, STEAM_DOT_Y);   // the background around (maybe the edge of the icon), without the dot

  // the heater goes on: the dot appears; off: it goes
  sc.showHeaterState(true);
  run(sc, 3);
  const int on = dotInk(STEAM_DOT_X, STEAM_DOT_Y);
  CHECK(on > off + 20);
  sc.showHeaterState(false);
  run(sc, 3);
  CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == off);

  // it blinks with the heater, and the temperature drawn again (one, two and three digits, ERR) never leaves it cut
  const double temperatures[] = {95.0, 101.0, 99.0, 134.6, 135.2, 8.0, 140.0};
  for (int i = 0; i < 7; i++) {
    sc.showHeaterState(true);
    t += 1100;
    sc.showLiveTemperature(t, temperatures[i]);
    run(sc, 3);
    CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == on);
  }
  sc.showTemperatureError();
  run(sc, 3);
  CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == on);
  t += 1100;
  sc.showLiveTemperature(t, 136.0);
  run(sc, 3);
  CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == on);

  // the place of the dot of the other modes stays empty in the steaming mode
  CHECK(dotInk(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y) == 0);

  // ---- setting the temperature fades as a change of parameter does: the first turn fades the measured temperature out and the setpoint in,
  // the next turns change the number, and the measured temperature comes back with a fade after STEAM_SETPOINT_SHOW_MS
  sc.showHeaterState(true);
  t += 1100;
  sc.showLiveTemperature(t, 128.0);
  run(sc, 3);
  const Picture measured = snapshot();
  CHECK(!sc.animating());
  CHECK(sc.adjustActiveParameter(1));                  // 135 -> 136: the setpoint is shown
  CHECK(sc.animating());
  run(sc, 4);                                          // within the fade out: the number is darker than the text
  CHECK(brightestIn() < COLOR_TEXT_LEVEL);
  CHECK(sc.adjustActiveParameter(1));                  // 137, during the fade: no new fade, the number follows
  for (int i = 0; i < 100 && sc.animating(); i++) run(sc, 1);
  CHECK(!sc.animating());
  run(sc, 3);
  CHECK(brightestIn() == COLOR_TEXT_LEVEL);            // at full brightness at the end
  CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == on);       // the dot is whole
  const Picture setpoint = snapshot();
  CHECK(setpoint != measured);
  CHECK(completeRedraw(sc) == setpoint);               // the setpoint 137, as a complete redraw shows it
  // the next turn changes the number without a fade
  CHECK(sc.adjustActiveParameter(1));
  CHECK(!sc.animating());
  run(sc, 3);
  // after STEAM_SETPOINT_SHOW_MS without a turn the measured temperature comes back, with a fade
  for (int i = 0; i < (int)(STEAM_SETPOINT_SHOW_MS / 10) + 5 && !sc.animating(); i++) run(sc, 1);
  CHECK(sc.animating());
  for (int i = 0; i < 100 && sc.animating(); i++) run(sc, 1);
  run(sc, 3);
  CHECK(snapshot() == measured);                       // the measured temperature (128) again, as before the turn
  CHECK(dotInk(STEAM_DOT_X, STEAM_DOT_Y) == on);

  // back to the brewing: the dot is at its usual place again
  sc.leaveSteaming();
  settle(sc);
  CHECK(!sc.steaming());
  CHECK(dotInk(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y) > 20);

  printf(fails ? "%d FAIL\n" : "test_steam_dot: ALL OK\n", fails);
  return fails != 0;
}
