// SPDX-License-Identifier: MIT
// transform suite: rigid at angle 0 is the identity translation; flipX
// mirrors x only; rigid at 90 degrees maps the unit axes as the convention
// says (x right, y down, positive angles clockwise on the panel: +x turns to
// +y); compose applies the child first; compose then inverse is the
// identity to float tolerance; a singular matrix inverts to zeros.

#include <math.h>

#include "SD_Xform.h"

#include "test_common.h"
#include "test_fixtures.h"

static bool near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

static bool nearIdentity(const SD_Xform &m, float tol) {
  return near(m.a, 1.0f, tol) && near(m.b, 0.0f, tol) && near(m.c, 0.0f, tol) &&
         near(m.d, 1.0f, tol) && near(m.tx, 0.0f, tol) && near(m.ty, 0.0f, tol);
}

static SD_Xform randomAffine() {
  SD_Xform m;
  m.a = frand(-3.0f, 3.0f);
  m.b = frand(-3.0f, 3.0f);
  m.c = frand(-3.0f, 3.0f);
  m.d = frand(-3.0f, 3.0f);
  m.tx = frand(-200.0f, 200.0f);
  m.ty = frand(-200.0f, 200.0f);
  return m;
}

void runXformTests() {
  // ---- rigid at angle 0: the identity matrix, moved to (x, y) ----
  {
    const SD_Xform m = SD_Xform::rigid(12.0f, -7.5f, 0.0f);
    CHECK(m.a == 1.0f && m.b == 0.0f && m.c == 0.0f && m.d == 1.0f,
          "rigid(0): matrix {%g %g %g %g} is not the identity", m.a, m.b, m.c, m.d);
    CHECK(m.tx == 12.0f && m.ty == -7.5f, "rigid(0): translation (%g, %g)", m.tx, m.ty);
    float x, y;
    m.apply(3.0f, 4.0f, &x, &y);
    CHECK(x == 15.0f && y == -3.5f, "rigid(0): (3, 4) went to (%g, %g)", x, y);
  }

  // ---- flipX mirrors x only ----
  {
    const SD_Xform m = SD_Xform::rigid(10.0f, 20.0f, 0.0f, true);
    float x, y;
    m.apply(1.0f, 0.0f, &x, &y);
    CHECK(x == 9.0f && y == 20.0f, "flip(0): +x went to (%g, %g), want (9, 20)", x, y);
    m.apply(0.0f, 1.0f, &x, &y);
    CHECK(x == 10.0f && y == 21.0f, "flip(0): +y went to (%g, %g), want (10, 21)", x, y);
    m.apply(-4.0f, -2.0f, &x, &y);
    CHECK(x == 14.0f && y == 18.0f, "flip(0): (-4, -2) went to (%g, %g)", x, y);
  }

  // ---- rigid at 90 degrees: +x turns to +y (down), +y to -x ----
  {
    const SD_Xform m = SD_Xform::rigid(0.0f, 0.0f, degToRad(90.0f));
    float x, y;
    m.apply(1.0f, 0.0f, &x, &y);
    CHECK(near(x, 0.0f, 1e-6f) && near(y, 1.0f, 1e-6f), "rigid(90): +x went to (%g, %g)", x, y);
    m.apply(0.0f, 1.0f, &x, &y);
    CHECK(near(x, -1.0f, 1e-6f) && near(y, 0.0f, 1e-6f), "rigid(90): +y went to (%g, %g)", x, y);
    // a small positive turn is clockwise on a y-down panel: +x dips down
    const SD_Xform s = SD_Xform::rigid(0.0f, 0.0f, 0.1f);
    s.apply(1.0f, 0.0f, &x, &y);
    CHECK(y > 0.0f, "positive angle should turn +x toward +y (clockwise), got y %g", y);
    // mirrored at 90: +x mirrors to -x, then turns to -y
    const SD_Xform f = SD_Xform::rigid(0.0f, 0.0f, degToRad(90.0f), true);
    f.apply(1.0f, 0.0f, &x, &y);
    CHECK(near(x, 0.0f, 1e-6f) && near(y, -1.0f, 1e-6f), "flip(90): +x went to (%g, %g)", x, y);
    f.apply(0.0f, 1.0f, &x, &y);
    CHECK(near(x, -1.0f, 1e-6f) && near(y, 0.0f, 1e-6f), "flip(90): +y went to (%g, %g)", x, y);
  }

  // ---- rigid matches its documented formula at many angles ----
  {
    long bad = 0;
    for (int k = 0; k < 200; ++k) {
      const float ang = frand(-7.0f, 7.0f);
      const float px = frand(-50.0f, 50.0f), py = frand(-50.0f, 50.0f);
      const float dx = frand(-20.0f, 20.0f), dy = frand(-20.0f, 20.0f);
      const bool flip = (k & 1) != 0;
      float x, y;
      SD_Xform::rigid(px, py, ang, flip).apply(dx, dy, &x, &y);
      const float fx = flip ? -dx : dx;
      const float wx = px + cosf(ang) * fx - sinf(ang) * dy;
      const float wy = py + sinf(ang) * fx + cosf(ang) * dy;
      if (!near(x, wx, 1e-4f) || !near(y, wy, 1e-4f)) ++bad;
    }
    CHECK(bad == 0, "rigid disagrees with x + cos dx - sin dy at %ld of 200 points", bad);
  }

  // ---- compose: child first, then parent ----
  {
    long bad = 0;
    for (int k = 0; k < 200; ++k) {
      const SD_Xform p = randomAffine(), q = randomAffine();
      const SD_Xform pq = SD_Xform::compose(p, q);
      const float vx = frand(-30.0f, 30.0f), vy = frand(-30.0f, 30.0f);
      float ax, ay, bx, by, cx, cy;
      pq.apply(vx, vy, &ax, &ay);
      q.apply(vx, vy, &bx, &by);
      p.apply(bx, by, &cx, &cy);
      const float tol = 1e-3f * (1.0f + fabsf(cx) + fabsf(cy));
      if (!near(ax, cx, tol) || !near(ay, cy, tol)) ++bad;
    }
    CHECK(bad == 0, "compose(p, q) is not p after q at %ld of 200 points", bad);
  }

  // ---- compose then inverse is the identity ----
  {
    long bad = 0;
    for (int k = 0; k < 200; ++k) {
      const SD_Xform m = k < 100 ? SD_Xform::rigid(frand(-200.0f, 200.0f), frand(-200.0f, 200.0f),
                                                   frand(-7.0f, 7.0f), (k & 1) != 0)
                                 : randomAffine();
      const float det = m.a * m.d - m.b * m.c;
      if (fabsf(det) < 0.05f) continue;  // near-singular: tolerance means nothing there
      const SD_Xform inv = m.inverse();
      const float tol = k < 100 ? 1e-4f : 1e-3f;
      if (!nearIdentity(SD_Xform::compose(m, inv), tol)) ++bad;
      if (!nearIdentity(SD_Xform::compose(inv, m), tol)) ++bad;
      // and a point there and back
      float x, y, bx, by;
      const float vx = frand(-40.0f, 40.0f), vy = frand(-40.0f, 40.0f);
      m.apply(vx, vy, &x, &y);
      inv.apply(x, y, &bx, &by);
      if (!near(bx, vx, 0.05f) || !near(by, vy, 0.05f)) ++bad;
    }
    CHECK(bad == 0, "inverse is not the identity under compose at %ld cases", bad);
  }

  // ---- a matrix with no finite inverse inverts to zeros ----
  {
    const SD_Xform singular[] = {
        {0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 5.0f},
        {1.0f, 2.0f, 2.0f, 4.0f, 0.0f, 0.0f},
        {NAN, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f, INFINITY, 0.0f},
        {1e-30f, 0.0f, 0.0f, 1e-30f, 0.0f, 0.0f},
    };
    for (const SD_Xform &m : singular) {
      const SD_Xform inv = m.inverse();
      CHECK(inv.a == 0.0f && inv.b == 0.0f && inv.c == 0.0f && inv.d == 0.0f &&
                inv.tx == 0.0f && inv.ty == 0.0f,
            "matrix {%g %g %g %g %g %g} should invert to zeros", m.a, m.b, m.c, m.d, m.tx,
            m.ty);
    }
  }
}
