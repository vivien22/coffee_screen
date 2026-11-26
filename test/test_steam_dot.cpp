// The heater dot in the steaming mode: next to the thermometer, as in the other modes, it follows the heater (it blinks with it) and it
// stays whole when the steaming temperature is drawn again (the zone of that temperature reaches the edge of the dot). The real
// ScreenController and Display.cpp draw into the frame buffer of the GFX stub.
#include <cstdio>
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

  // back to the brewing: the dot is at its usual place again
  sc.leaveSteaming();
  settle(sc);
  CHECK(!sc.steaming());
  CHECK(dotInk(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y) > 20);

  printf(fails ? "%d FAIL\n" : "test_steam_dot: ALL OK\n", fails);
  return fails != 0;
}
