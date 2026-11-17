// The number fields redraw only what changed (no flicker). Property: whatever the history of the updates of a
// field, the picture is the same as drawing the final value from scratch. The frame buffer is the one of the
// GFX stub (the real font rules), Display.cpp is the real one.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "src/display/Display.h"

uint16_t frameBuffer[240 * 240];
unsigned long millis() { return 0; }
void delay(unsigned long) {}
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return 0; }
char *itoa(int v, char *s, int) { sprintf(s, "%d", v); return s; }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

typedef std::vector<uint16_t> Picture;
// Only the pixels inside the round screen count: what is drawn outside is not visible
static Picture snapshot() {
  Picture p(frameBuffer, frameBuffer + 240 * 240);
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      const int dx = x - 120, dy = y - 120;
      if (dx * dx + dy * dy > 120 * 120) p[y * 240 + x] = 0;
    }
  }
  return p;
}

// Random update of the three fields: the kinds of call the screen controller makes
struct Call { int field; int value; int size; int tag; bool redraw; };

static Call randomCall(int field, int lastTag) {
  Call c;
  c.field = field;
  c.size = rand() % 2;
  if (field == 0) {                 // temperature: tenths of degree, 15.0 .. 119.9 (the unit of a 3 digit large
    c.value = 150 + rand() % 1050;  // number can pass the edge of the zone, past 120 degrees nobody looks)
    c.tag = 0;
    c.redraw = false;
  } else {
    // Values with 1, 2 and 3 digits; the tags of the field (volume: PRE, CAL; duration: WAIT, FLUSH)
    c.value = (rand() % 3 == 0) ? rand() % 10 : (rand() % 3 == 0 ? rand() % 1000 : rand() % 100);
    const int r = rand() % 4;
    if (field == 1) c.tag = (r == 0) ? PHASE_TAG_PREINFUSION : (r == 1 ? PHASE_TAG_CALIBRATION : PHASE_TAG_NONE);
    else            c.tag = (r == 0) ? PHASE_TAG_PREINFUSION_WAIT : (r == 1 ? PHASE_TAG_FLUSH : PHASE_TAG_NONE);
    // Next to a tag, large digits have 2 digits at most (the pre-infusion volume is at most 50 ml): a third one
    // would overlap the tag
    if (c.tag != PHASE_TAG_NONE) c.value %= (field == 1 ? 51 : 10);   // next to a tag: PRE up to 50 ml, WAIT / FLUSH up to 9 s
    // The tag is redrawn when it changes (the controller does it), and sometimes for no reason
    c.redraw = (c.tag != lastTag) || (rand() % 4 == 0);
  }
  return c;
}

static void apply(const Call &c) {
  if (c.field == 0) drawTemperature(c.value / 10.0, c.size ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
  else if (c.field == 1) drawVolume(c.value, c.size ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL, (PhaseTag)c.tag, c.redraw);
  else drawDuration(c.value, c.size ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL, (PhaseTag)c.tag, c.redraw);
}

int main() {
  srand(12345);
  int sequences = 0;
  for (int field = 0; field < 3; field++) {
    for (int n = 0; n < 400; n++, sequences++) {
      // History of updates, then the picture, then the same final call on a screen that starts empty
      beginDisplay();
      int lastTag = 0;
      std::vector<Call> history;
      Call last = randomCall(field, lastTag);
      const int steps = 1 + rand() % 12;
      for (int i = 0; i < steps; i++) {
        last = randomCall(field, lastTag);
        // The temperature goes through ERR from time to time
        if (field == 0 && rand() % 8 == 0) drawTemperatureError();
        apply(last);
        history.push_back(last);
        lastTag = last.tag;
      }
      const Picture incremental = snapshot();

      beginDisplay();
      last.redraw = true;
      apply(last);
      const Picture fresh = snapshot();

      if (incremental != fresh) {
        int diff = 0, x0 = 240, x1 = -1, y0 = 240, y1 = -1;
        for (size_t i = 0; i < fresh.size(); i++) {
          if (incremental[i] != fresh[i]) {
            diff++;
            x0 = (int)(i % 240) < x0 ? (int)(i % 240) : x0; x1 = (int)(i % 240) > x1 ? (int)(i % 240) : x1;
            y0 = (int)(i / 240) < y0 ? (int)(i / 240) : y0; y1 = (int)(i / 240) > y1 ? (int)(i / 240) : y1;
          }
        }
        printf("   differences in x %d..%d, y %d..%d\n", x0, x1, y0, y1);
        printf("FAIL field %d sequence %d: %d pixels differ (last: value %d size %d tag %d)\n",
               field, n, diff, last.value, last.size, last.tag);
        for (size_t i = 0; i < history.size(); i++) {
          printf("   %d: value %d size %d tag %d redraw %d\n", (int)i, history[i].value, history[i].size, history[i].tag, history[i].redraw);
        }
        if (++fails > 5) return 1;
      }
    }
  }
  printf("%d sequences: %s\n", sequences, fails ? "FAILED" : "ok");
  return fails != 0;
}
