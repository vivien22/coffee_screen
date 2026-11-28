// DisplayArc.cpp: the progress arc around the screen (see Display.h), and the erasing of the zones of the fields that never touches it.

#include <math.h>
#include "Display.h"
#include "DisplayText.h"
#include "Layout.h"
#include "Screen.h"

// The progress arc (see Display.h). The center of the screen is at (120, 120) in pixel edges; a pixel is seen from its center.
static const float ARC_CENTER = 120.0f;
static const float ARC_TWO_PI = 6.2831853f;
static const uint8_t ARC_RUN_MAX = 16;   // pixels of a run sent at once (a buffer full is sent, the run goes on in a new window)

void drawArcSegments(uint8_t first, uint8_t last) {
  const float outer = ARC_OUTER_RADIUS, inner = ARC_OUTER_RADIUS - ARC_WIDTH;
  if (first >= last) return;
  {
    // The box of the range: the four corners of each of its segments (so the top, bottom, left and right of the ring are
    // found when the range crosses them), and a pixel more all around
    float left = 1000, right = -1000, top = 1000, bottom = -1000;
    for (uint8_t k = first; k < last; k++) {
      for (uint8_t corner = 0; corner < 4; corner++) {
        const float angle = (k + (corner & 1)) * ARC_TWO_PI / ARC_SEGMENTS;
        const float radius = (corner & 2) ? inner : outer;
        const float x = ARC_CENTER + radius * sinf(angle), y = ARC_CENTER - radius * cosf(angle);
        left = x < left ? x : left;  right = x > right ? x : right;
        top = y < top ? y : top;     bottom = y > bottom ? y : bottom;
      }
    }
    // Each row of the box is drawn by runs: the consecutive pixels of the range are one window of one row, then their colors
    // (a window costs several command bytes, a pixel two). A row holds several runs where it leaves the range.
    uint16_t colors[ARC_RUN_MAX];
    tft.startWrite();
    for (int16_t y = (int16_t)top - 1; y <= (int16_t)bottom + 1; y++) {
      // Only the pixels near the ring are tested: the row stops at the outer circle and jumps over the inside of the inner one
      // (a pixel of the ring has its center between inner - 0.5 and outer + 0.5; a pixel more all around)
      const float dy = y + 0.5f - ARC_CENTER;
      const float outerReach = outer + 0.5f, innerReach = inner - 0.5f;
      if (dy >= outerReach || dy <= -outerReach) continue;
      const float outerHalf = sqrtf(outerReach * outerReach - dy * dy);
      const float innerHalf = (dy > -innerReach && dy < innerReach) ? sqrtf(innerReach * innerReach - dy * dy) - 1.0f : 0.0f;   // pixels with |dx| < this have no coverage
      int16_t xFirst = (int16_t)floorf(ARC_CENTER - outerHalf) - 1, xLast = (int16_t)ceilf(ARC_CENTER + outerHalf) + 1;
      if (xFirst < (int16_t)left - 1) xFirst = (int16_t)left - 1;
      if (xLast > (int16_t)right + 1) xLast = (int16_t)right + 1;
      uint8_t count = 0;
      int16_t runX = 0;
      for (int16_t x = xFirst; x <= xLast + 1; x++) {   // the last x is outside: it ends the run
        uint8_t opacity = 0;
        bool jump = false;
        if (x <= xLast) {
          const float dx = x + 0.5f - ARC_CENTER;
          if (dx > -innerHalf && dx < innerHalf) {   // inside the inner circle: nothing here, go to its other side
            jump = true;
          } else {
            const float radius = sqrtf(dx * dx + dy * dy);
            const float coverage = (radius - inner < outer - radius ? radius - inner : outer - radius) + 0.5f;   // smooth edges: 1 px wide ramp
            if (coverage > 0.0f) {
              float angle = atan2f(dx, -dy);   // from 12 o'clock, clockwise
              if (angle < 0.0f) angle += ARC_TWO_PI;
              uint16_t segment = (uint16_t)(angle * ARC_SEGMENTS / ARC_TWO_PI);
              if (segment >= ARC_SEGMENTS) segment = 0;
              if (segment >= first && segment < last) {
                const float level = (coverage >= 1.0f ? 1.0f : coverage) * ARC_BRIGHTNESS_PERCENT / 100.0f;   // edges and body, a little darker than the icons
                opacity = (uint8_t)(level * 15.0f + 0.5f);
              }
            }
          }
        }
        if (opacity) {
          if (!count) runX = x;
          colors[count++] = BLEND_COLOR(opacity);
        }
        if (count && (!opacity || count == ARC_RUN_MAX)) {   // the run ends here: send it
          tft.setAddrWindow(runX, y, count, 1);
          tft.pushPixels(colors, count);
          count = 0;
        }
        if (jump) x = (int16_t)floorf(ARC_CENTER + innerHalf - 0.5f);   // the next pixel is the first one outside
      }
    }
    tft.endWrite();
  }
}

void eraseArc() {
  const float outer = ARC_OUTER_RADIUS + 1.0f, inner = ARC_OUTER_RADIUS - ARC_WIDTH - 1.0f;   // a pixel more than the ring
  for (int16_t y = 0; y < 240; y++) {
    const float dy = y + 0.5f - ARC_CENTER;
    if (dy >= outer || dy <= -outer) continue;
    int16_t outerHalf = (int16_t)ceilf(sqrtf(outer * outer - dy * dy));
    if (outerHalf > 120) outerHalf = 120;
    if (dy > -inner && dy < inner) {   // the two ends of the row, left of the inner circle and right of it
      const int16_t innerHalf = (int16_t)floorf(sqrtf(inner * inner - dy * dy));
      tft.fillRect(120 - outerHalf, y, outerHalf - innerHalf, 1, COLOR_BG);
      tft.fillRect(120 + innerHalf, y, outerHalf - innerHalf, 1, COLOR_BG);
    } else {
      tft.fillRect(120 - outerHalf, y, 2 * outerHalf, 1, COLOR_BG);
    }
  }
}

// Erases a zone of a field (a rectangle of COLOR_BG) except where the progress arc may be: each row stops at the circle inside the ring, so
// that the ring is never erased with the zone of a number. Everything that a zone holds is inside that circle (the farthest point
// is the end of the tag FLUSH, at 112.9 from the middle, the ring starts at 114.5).
void eraseZone(int16_t x, int16_t y, int16_t width, int16_t height) {
  const float safe = ARC_OUTER_RADIUS - ARC_WIDTH - 0.5f;   // the pixels of the ring start beyond it
  for (int16_t row = y; row < y + height; row++) {
    const float dy = row + 0.5f - ARC_CENTER;
    if (dy >= safe || dy <= -safe) continue;
    int16_t right = (int16_t)(ARC_CENTER + 0.5f + sqrtf(safe * safe - dy * dy));   // the pixels left of it have their center inside
    if (right > x + width) right = x + width;
    if (right > x) tft.fillRect(x, row, right - x, 1, COLOR_BG);
  }
}
