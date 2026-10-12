// SPDX-License-Identifier: MIT

#include "SD_IK.h"

#include <float.h>
#include <math.h>

static const float kPi = 3.14159265358979f;

// true for every float but nan and the infinities.
static inline bool ikFinite(float v) { return v >= -FLT_MAX && v <= FLT_MAX; }

bool sd_ik2(float l1, float l2, float tx, float ty, float bendSign, float &a1, float &a2) {
  if (!ikFinite(l1) || !ikFinite(l2) || !(l1 > 0.0f) || !(l2 > 0.0f)) return false;
  if (!ikFinite(tx) || !ikFinite(ty)) return false;
  const float side = bendSign < 0.0f ? -1.0f : 1.0f;

  const float d2 = tx * tx + ty * ty;
  const float d = sqrtf(d2);
  if (d == 0.0f) {
    a2 = side * kPi;
    return false;
  }
  const float at = atan2f(ty, tx);

  // out of reach: straight at the target
  if (d > l1 + l2) {
    a1 = at;
    a2 = 0.0f;
    return false;
  }

  // too close: folded, the tip on the ray toward the target. with the
  // second link the longer, the first points away so the tip points at it.
  if (d < fabsf(l1 - l2)) {
    if (l1 >= l2)
      a1 = at;
    else
      a1 = at > 0.0f ? at - kPi : at + kPi;
    a2 = side * kPi;
    return false;
  }

  // reached: the elbow by the law of cosines, then the first link turned
  // back by the angle the second link's tip makes with it.
  float c2 = (d2 - l1 * l1 - l2 * l2) / (2.0f * l1 * l2);
  if (c2 < -1.0f) c2 = -1.0f;
  if (c2 > 1.0f) c2 = 1.0f;
  a2 = side * acosf(c2);
  a1 = at - atan2f(l2 * sinf(a2), l1 + l2 * cosf(a2));
  return true;
}
