// render_screens: draws the screens of the machine with the real display code (Display.cpp, ScreenController.cpp)
// into frame buffers and writes them as raw RGB565 files (240 x 240). tools/render_screens.sh turns them into PNG.
//
//   usage: render_screens <output folder>

#include <cstdio>
#include <string>
#include "src/Config.h"
#include "src/display/ScreenController.h"

uint16_t frameBuffer[240 * 240];

// The Arduino API that the display code needs is the one of the tests (test/stubs/ArduinoStubs.cpp);
// the clock of the screens is its clock
static unsigned long &now_ = stubNow;

static std::string outDir;

// Saves the frame buffer as <name>.rgb565 (little endian 16 bit pixels)
static void save(const char *name) {
  const std::string path = outDir + "/" + name + ".rgb565";
  FILE *f = fopen(path.c_str(), "wb");
  fwrite(frameBuffer, sizeof(frameBuffer), 1, f);
  fclose(f);
}

static Settings settings(bool manual = false) {
  Settings s;
  s.temperature = 93;
  s.volume = 40;
  s.preinfusion = 10;
  s.preinfusionWait = 5;
  s.flush = 5;
  s.manualSeconds = 0;
  s.loss = DEFAULT_LOSS_ML;
  s.manual = manual;
  return s;
}

// A screen at rest: 92.4 degrees, heater on
static void ready(ScreenController &sc, bool manual = false) {
  sc.begin(settings(manual));
  sc.finishWelcome();   // the start-up sequence: the picture, then the icons
  sc.showLiveTemperature(0, 92.4);
  sc.showHeaterState(true);
  now_ += 2000;
  sc.update(now_);
}

static void redraw(ScreenController &sc) {
  now_ += 2000;
  sc.update(now_);
}

int main(int argc, char **argv) {
  outDir = argc > 1 ? argv[1] : ".";

  { ScreenController sc; ready(sc); save("auto-ready"); }

  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showPreinfusion(7, 3); redraw(sc); save("auto-preinfusion"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showPreinfusionWait(10, 4); redraw(sc); save("auto-wait"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showExtractionProgress(26, 19); redraw(sc); save("auto-running"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showExtractionProgress(40, 27); redraw(sc); save("auto-result"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showFlush(40, 27, false); redraw(sc); save("auto-flush-wait"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showFlush(40, 4, true); redraw(sc); save("auto-flushing"); }

  // Settings: a double push selects the next parameter
  { ScreenController sc; ready(sc); sc.selectNextParameter(); redraw(sc); save("set-preinfusion"); }
  { ScreenController sc; ready(sc); sc.selectNextParameter(); sc.selectNextParameter(); redraw(sc); save("set-wait"); }
  { ScreenController sc; ready(sc); for (int i = 0; i < 3; i++) sc.selectNextParameter(); redraw(sc); save("set-flush"); }
  { ScreenController sc; ready(sc); for (int i = 0; i < 4; i++) sc.selectNextParameter(); redraw(sc); save("set-temperature"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showExtractionProgress(38, 27); redraw(sc);
    sc.offerCalibration(38); sc.selectNextParameter(); redraw(sc); save("set-calibration"); }
  { ScreenController sc; ready(sc); sc.adjustActiveParameter(5); redraw(sc); save("set-volume"); }

  // MANUAL mode
  { ScreenController sc; ready(sc, true); save("manual-ready"); }
  { ScreenController sc; ready(sc, true); sc.showExtractionStarted(); sc.showExtractionProgress(18, 14); redraw(sc); save("manual-running"); }
  { ScreenController sc; ready(sc, true); sc.selectNextParameter(); sc.adjustActiveParameter(25); redraw(sc); save("manual-time"); }

  // Errors
  { ScreenController sc; ready(sc); sc.showTemperatureError(); redraw(sc); save("error-temperature"); }
  { ScreenController sc; ready(sc); sc.showExtractionStarted(); sc.showExtractionProgress(3, 6); redraw(sc);
    sc.showNoWater(true); redraw(sc); save("error-no-water"); }   // the drop is hidden (first half of the blinking)

  return 0;
}
