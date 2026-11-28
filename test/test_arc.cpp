// The progress arc (drawArcSegments / eraseArc of Display.cpp, see ARC_SEGMENTS in Config.h): a ring along the edge of the screen, clockwise
// from 12 o'clock. Checked: the ring stays between its two radii, each segment is where its angle says, the cuts do not matter (the whole
// ring at once = the ring in pieces), the whole ring is full, and eraseArc() gives back the clear screen.
#include <cstdio>
#include <cmath>
#include <vector>
#include "src/Config.h"
#include "src/display/Display.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

typedef std::vector<uint16_t> Picture;

static Picture snapshot() { return Picture(frameBuffer, frameBuffer + 240 * 240); }

static const double PI_ = 3.14159265358979;

// The angle of a pixel from 12 o'clock, clockwise, in degrees
static double angleOf(int x, int y) {
  double a = atan2(x + 0.5 - 120.0, -(y + 0.5 - 120.0)) * 180.0 / PI_;
  return a < 0 ? a + 360.0 : a;
}

int main() {
  beginDisplay();
  // the body of the ring: the icon color at ARC_BRIGHTNESS_PERCENT over the background (the most frequent color of a part of the ring)
  drawArcSegments(0, 8);
  uint16_t arcBody = 0;
  {
    int best = 0;
    for (int i = 0; i < 240 * 240; i++) {
      if (frameBuffer[i] == COLOR_BG) continue;
      int n = 0;
      for (int j = i; j < 240 * 240; j++) if (frameBuffer[j] == frameBuffer[i]) n++;
      if (n > best) { best = n; arcBody = frameBuffer[i]; }
    }
  }
  CHECK(arcBody != COLOR_ICON && arcBody != COLOR_BG);   // a little darker than the icons, not the background
  eraseArc();
  const Picture clear = snapshot();

  // 1. A part of the ring: only ring pixels, only in the angles of the segments
  drawArcSegments(0, ARC_SEGMENTS / 4);   // from 12 o'clock to 3 o'clock
  const Picture quarter = snapshot();
  int ink = 0, outside = 0, wrongAngle = 0, full = 0;
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      if (quarter[y * 240 + x] == clear[y * 240 + x]) continue;
      ink++;
      const double r = hypot(x + 0.5 - 120.0, y + 0.5 - 120.0);
      if (r < ARC_OUTER_RADIUS - ARC_WIDTH - 0.6 || r > ARC_OUTER_RADIUS + 0.6) outside++;
      if (angleOf(x, y) >= 90.0) wrongAngle++;
      if (quarter[y * 240 + x] == arcBody) full++;
    }
  }
  CHECK(ink > 700 && ink < 1200);   // a quarter of a ring of 5 px: about 2 pi 117.5 / 4 x 5 = 920 pixels
  CHECK(outside == 0);
  CHECK(wrongAngle == 0);
  CHECK(full > ink / 2);            // most of the pixels are of the full color, the edges are blended

  // 2. The cuts do not matter: 0..60 at once = 0..17, then 17..40, then 40..60
  eraseArc();
  CHECK(snapshot() == clear);
  drawArcSegments(0, 60);
  const Picture whole = snapshot();
  eraseArc();
  drawArcSegments(0, 17); drawArcSegments(17, 40); drawArcSegments(40, 60);
  CHECK(snapshot() == whole);
  eraseArc();
  CHECK(snapshot() == clear);

  // 3. The whole ring is closed: no gap between the last segment and the first, and the same pixels as in pieces
  drawArcSegments(0, ARC_SEGMENTS);
  const Picture ring = snapshot();
  int gaps = 0;
  for (int a = 0; a < 360; a++) {   // along the middle of the ring, every degree
    const double r = ARC_OUTER_RADIUS - ARC_WIDTH / 2;
    const int x = (int)floor(120.0 + r * sin(a * PI_ / 180.0)), y = (int)floor(120.0 - r * cos(a * PI_ / 180.0));
    if (ring[y * 240 + x] != arcBody) gaps++;
  }
  CHECK(gaps == 0);
  eraseArc();
  CHECK(snapshot() == clear);

  // 4. The text of the screen is not touched by the erase of the ring (the screen is drawn, then the ring comes and goes)
  drawIcons();
  drawTemperature(92.4, TEXT_SIZE_SMALL);
  drawVolume(26, TEXT_SIZE_LARGE, PHASE_TAG_NONE, true);
  const Picture screen = snapshot();
  drawArcSegments(0, ARC_SEGMENTS);
  eraseArc();
  CHECK(snapshot() == screen);

  if (fails == 0) printf("test_arc: ALL OK\n");
  return fails ? 1 : 0;
}
