// SPDX-License-Identifier: MIT

#include "SD_Draw.h"

#include <math.h>

#include "SD_Internal.h"

// sample points as offsets from a target pixel centre.
static const float kSample1[1][2] = {{0.0f, 0.0f}};
static const float kSample2x2[4][2] = {
    {-0.25f, -0.25f}, {0.25f, -0.25f}, {-0.25f, 0.25f}, {0.25f, 0.25f}};

// the float expressions below are pinned by the golden hashes, in exactly
// this written form: the inverse from SD_Xform::inverse, the pivot added to
// its translation once (inv.tx + pivotX), each sample carried by
// SD_Xform::apply, rounded by floorf(u + 0.5f).
bool sd_draw(SD_Target &target, const SD_Sprite &s, const SD_Xform &m, SD_Sample sample) {
  if (!sd_validateSprite(s)) return false;

  const float (*pts)[2];
  int npts;
  if (sample == SD_SAMPLE_1) {
    pts = kSample1;
    npts = 1;
  } else if (sample == SD_SAMPLE_2x2) {
    pts = kSample2x2;
    npts = 4;
  } else {
    return false;
  }

  // target pixels to sprite pixels in one apply: the inverse matrix with the
  // pivot added to its translation. no finite inverse gives the zero matrix.
  SD_Xform inv = m.inverse();
  if (sdIsZero(inv)) return false;
  inv.tx = inv.tx + (float)s.pivotX;
  inv.ty = inv.ty + (float)s.pivotY;

  // the box, clipped to the target in float, before any conversion to int.
  // a nan edge clamps to the target's edge, so the conversions below always
  // see values inside the target.
  float bx0, by0, bx1, by1;
  sdBox(s, m, bx0, by0, bx1, by1);
  const int16_t tw = target.width();
  const int16_t th = target.height();
  if (!(bx0 >= 0.0f)) bx0 = 0.0f;
  if (!(by0 >= 0.0f)) by0 = 0.0f;
  if (!(bx1 <= (float)(tw - 1))) bx1 = (float)(tw - 1);
  if (!(by1 <= (float)(th - 1))) by1 = (float)(th - 1);
  if (bx0 > bx1 || by0 > by1) return true;

  const float sw = (float)s.w;
  const float sh = (float)s.h;
  const int xs = (int)bx0, xe = (int)bx1;
  const int ys = (int)by0, ye = (int)by1;
  for (int py = ys; py <= ye; ++py) {
    for (int px = xs; px <= xe; ++px) {
      bool hit = false;
      bool lit = false;
      for (int k = 0; k < npts; ++k) {
        float u, v;
        inv.apply((float)px + pts[k][0], (float)py + pts[k][1], &u, &v);
        const float fu = floorf(u + 0.5f);
        const float fv = floorf(v + 0.5f);
        // bounds in float first: a nan or huge sample is skipped, never
        // converted.
        if (!(fu >= 0.0f && fu < sw && fv >= 0.0f && fv < sh)) continue;
        bool ink;
        if (sd_spritePixel(s, (int16_t)fu, (int16_t)fv, ink)) {
          hit = true;
          if (ink) {
            lit = true;
            break;
          }
        }
      }
      if (hit) target.plot((int16_t)px, (int16_t)py, lit);
    }
  }
  return true;
}
