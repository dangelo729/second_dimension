// SPDX-License-Identifier: MIT

#include "SD_Xform.h"

#include <float.h>
#include <math.h>

// true for every float but nan and the infinities.
static inline bool sdFinite(float v) { return v >= -FLT_MAX && v <= FLT_MAX; }

SD_Xform SD_Xform::rigid(float x, float y, float angle, bool flipX) {
  const float cs = cosf(angle);
  const float sn = sinf(angle);
  SD_Xform m;
  m.a = flipX ? -cs : cs;
  m.b = -sn;
  m.c = flipX ? -sn : sn;
  m.d = cs;
  m.tx = x;
  m.ty = y;
  return m;
}

SD_Xform SD_Xform::scaled(float x, float y, float angle, bool flipX, float scale) {
  const float s = scale > 0.0f ? scale : 0.0f;
  SD_Xform m = rigid(x, y, angle, flipX);
  m.a = m.a * s;
  m.b = m.b * s;
  m.c = m.c * s;
  m.d = m.d * s;
  return m;
}

SD_Xform SD_Xform::compose(const SD_Xform &p, const SD_Xform &q) {
  SD_Xform m;
  m.a = p.a * q.a + p.b * q.c;
  m.b = p.a * q.b + p.b * q.d;
  m.c = p.c * q.a + p.d * q.c;
  m.d = p.c * q.b + p.d * q.d;
  m.tx = p.a * q.tx + p.b * q.ty + p.tx;
  m.ty = p.c * q.tx + p.d * q.ty + p.ty;
  return m;
}

SD_Xform SD_Xform::inverse() const {
  const SD_Xform zero = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  const float det = a * d - b * c;
  if (det == 0.0f) return zero;
  SD_Xform m;
  m.a = d / det;
  m.b = -b / det;
  m.c = -c / det;
  m.d = a / det;
  m.tx = -(m.a * tx + m.b * ty);
  m.ty = -(m.c * tx + m.d * ty);
  if (!sdFinite(m.a) || !sdFinite(m.b) || !sdFinite(m.c) || !sdFinite(m.d) ||
      !sdFinite(m.tx) || !sdFinite(m.ty))
    return zero;
  return m;
}
