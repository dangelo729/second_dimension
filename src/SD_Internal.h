// SPDX-License-Identifier: MIT
#ifndef SD_INTERNAL_H
#define SD_INTERNAL_H

// helpers the library's own .cpp files share. no public header includes it.

#include <math.h>

#include "SD_Config.h"
#include "SD_Sprite.h"
#include "SD_Xform.h"

// the zero matrix: what inverse() returns when there is no finite inverse,
// and what an unposed bone's world transform holds.
static const SD_Xform kSDZero = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// true when all six entries are zero.
static inline bool sdIsZero(const SD_Xform &m) {
  return m.a == 0.0f && m.b == 0.0f && m.c == 0.0f && m.d == 0.0f && m.tx == 0.0f &&
         m.ty == 0.0f;
}

// the box sd_draw visits for a sprite through m, before clipping: the
// sprite's pixel area about its pivot, through m by its four corners, its
// bounds floored and ceiled. the float expressions are pinned by the host
// suite's golden hashes.
static inline void sdBox(const SD_Sprite &s, const SD_Xform &m, float &bx0, float &by0,
                         float &bx1, float &by1) {
  const float x0 = -0.5f - (float)s.pivotX;
  const float y0 = -0.5f - (float)s.pivotY;
  const float x1 = (float)s.w - 0.5f - (float)s.pivotX;
  const float y1 = (float)s.h - 0.5f - (float)s.pivotY;
  float cx[4], cy[4];
  m.apply(x0, y0, &cx[0], &cy[0]);
  m.apply(x1, y0, &cx[1], &cy[1]);
  m.apply(x0, y1, &cx[2], &cy[2]);
  m.apply(x1, y1, &cx[3], &cy[3]);
  float minx = cx[0], maxx = cx[0], miny = cy[0], maxy = cy[0];
  for (int k = 1; k < 4; ++k) {
    if (cx[k] < minx) minx = cx[k];
    if (cx[k] > maxx) maxx = cx[k];
    if (cy[k] < miny) miny = cy[k];
    if (cy[k] > maxy) maxy = cy[k];
  }
  bx0 = floorf(minx);
  by0 = floorf(miny);
  bx1 = ceilf(maxx);
  by1 = ceilf(maxy);
}

#endif  // SD_INTERNAL_H
