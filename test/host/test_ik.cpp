// SPDX-License-Identifier: MIT
// ik suite: for a grid of reachable targets, both elbows, and three pairs of
// link lengths (equal, the first longer, the second longer), forward
// kinematics of sd_ik2's angles (in double) lands on the target within 1e-4
// and the elbow sits on the side bendSign asks; the reach's two edges are
// reached; out of reach points the chain straight at the target with
// a2 = 0; too close folds it with the tip on the ray toward the target;
// d == 0 keeps a1 and folds a2; bad lengths and targets return false and
// leave the angles alone; and the angles posed as a chain under a plain or
// mirrored shoulder joint in an SD_Rig put the tip on the target.

#include <math.h>

#include "SD_IK.h"
#include "SD_Rig.h"

#include "test_common.h"

static const double kTol = 1e-4;
static const float kPiF = 3.14159265358979f;

static void tip(double l1, double l2, float a1, float a2, double *x, double *y) {
  *x = l1 * cos((double)a1) + l2 * cos((double)a1 + (double)a2);
  *y = l1 * sin((double)a1) + l2 * sin((double)a1 + (double)a2);
}

void runIkTests() {
  struct Lengths {
    float l1, l2;
  };
  static const Lengths pairs[] = {{15.0f, 15.0f}, {15.0f, 12.0f}, {5.0f, 9.0f}};

  // ---- a grid of targets: reached, out of reach, too close ----
  for (const Lengths &L : pairs) {
    const double l1 = L.l1, l2 = L.l2;
    const double outer = l1 + l2, inner = fabs(l1 - l2);
    for (int side = 0; side < 2; ++side) {
      const float bend = side == 0 ? 1.0f : -1.0f;
      long reached = 0, missed = 0, wrongSide = 0, wrongReturn = 0;
      long far = 0, farBad = 0, close = 0, closeBad = 0;
      double worst = 0.0;
      for (int iy = -40; iy <= 40; ++iy)
        for (int ix = -40; ix <= 40; ++ix) {
          const float tx = (float)ix * 0.8f, ty = (float)iy * 0.8f;
          const double d = sqrt((double)tx * tx + (double)ty * ty);
          float a1 = 0.0f, a2 = 0.0f;
          const bool ok = sd_ik2(L.l1, L.l2, tx, ty, bend, a1, a2);
          double x, y;
          tip(l1, l2, a1, a2, &x, &y);
          if (d > inner + 1e-3 && d < outer - 1e-3) {
            ++reached;
            if (!ok) ++wrongReturn;
            const double err = hypot(x - tx, y - ty);
            if (err > worst) worst = err;
            if (err > kTol) ++missed;
            // the elbow at positive a2 is anticlockwise of the line to the
            // target on the panel: (target x elbow) < 0 in a y-down frame
            const double ex = l1 * cos((double)a1), ey = l1 * sin((double)a1);
            const double cross = (double)tx * ey - (double)ty * ex;
            const bool asked = bend > 0 ? (a2 > 0.0f && cross < 0.0) : (a2 < 0.0f && cross > 0.0);
            if (!asked) ++wrongSide;
          } else if (d > outer + 1e-3) {
            ++far;
            const double wx = outer * tx / d, wy = outer * ty / d;
            if (ok || a2 != 0.0f || a1 != atan2f(ty, tx) || hypot(x - wx, y - wy) > kTol)
              ++farBad;
          } else if (d > 1e-3 && d < inner - 1e-3) {
            ++close;
            const double wx = inner * tx / d, wy = inner * ty / d;
            if (ok || a2 != bend * kPiF || hypot(x - wx, y - wy) > kTol) ++closeBad;
          }
        }
      CHECK(reached > 500, "l1 %g l2 %g: only %ld reachable grid targets", l1, l2, reached);
      CHECK(wrongReturn == 0, "l1 %g l2 %g bend %g: %ld reachable targets returned false", l1, l2,
            (double)bend, wrongReturn);
      CHECK(missed == 0, "l1 %g l2 %g bend %g: %ld of %ld tips off the target (worst %g)", l1, l2,
            (double)bend, missed, reached, worst);
      CHECK(wrongSide == 0, "l1 %g l2 %g bend %g: %ld elbows on the wrong side", l1, l2,
            (double)bend, wrongSide);
      CHECK(far > 1000 && farBad == 0,
            "l1 %g l2 %g bend %g: %ld of %ld out-of-reach targets not pointed straight at", l1, l2,
            (double)bend, farBad, far);
      if (inner > 0.0)
        CHECK(close > 0 && closeBad == 0,
              "l1 %g l2 %g bend %g: %ld of %ld too-close targets not folded toward", l1, l2,
              (double)bend, closeBad, close);
    }
  }

  // ---- the reach's edges: straight at l1 + l2, folded at |l1 - l2| ----
  for (const Lengths &L : pairs) {
    const float dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    for (const auto &u : dirs) {
      float a1 = 0.0f, a2 = 0.0f;
      const float outer = L.l1 + L.l2;
      const bool okOut = sd_ik2(L.l1, L.l2, u[0] * outer, u[1] * outer, 1.0f, a1, a2);
      double x, y;
      tip(L.l1, L.l2, a1, a2, &x, &y);
      CHECK(okOut && a2 == 0.0f && hypot(x - u[0] * outer, y - u[1] * outer) <= kTol,
            "l1 %g l2 %g: the outer edge (%g, %g) not reached straight (a2 %g)", (double)L.l1,
            (double)L.l2, (double)(u[0] * outer), (double)(u[1] * outer), (double)a2);
      const float inner = fabsf(L.l1 - L.l2);
      if (inner == 0.0f) continue;
      const bool okIn = sd_ik2(L.l1, L.l2, u[0] * inner, u[1] * inner, -1.0f, a1, a2);
      tip(L.l1, L.l2, a1, a2, &x, &y);
      CHECK(okIn && fabsf(a2) > 3.14f && hypot(x - u[0] * inner, y - u[1] * inner) <= kTol,
            "l1 %g l2 %g: the inner edge (%g, %g) not reached folded (a2 %g)", (double)L.l1,
            (double)L.l2, (double)(u[0] * inner), (double)(u[1] * inner), (double)a2);
    }
  }

  // ---- d == 0: a1 kept as passed in, a2 folded by bendSign, false ----
  for (const Lengths &L : pairs) {
    float a1 = 0.7f, a2 = 0.0f;
    const bool okPos = sd_ik2(L.l1, L.l2, 0.0f, 0.0f, 1.0f, a1, a2);
    CHECK(!okPos && a1 == 0.7f && a2 == kPiF, "d == 0, bend +1: returned %d, a1 %g, a2 %g",
          (int)okPos, (double)a1, (double)a2);
    a1 = -1.2f;
    const bool okNeg = sd_ik2(L.l1, L.l2, 0.0f, 0.0f, -1.0f, a1, a2);
    CHECK(!okNeg && a1 == -1.2f && a2 == -kPiF, "d == 0, bend -1: returned %d, a1 %g, a2 %g",
          (int)okNeg, (double)a1, (double)a2);
  }

  // ---- bendSign zero counts as positive ----
  {
    float a1 = 0.0f, a2 = 0.0f;
    CHECK(sd_ik2(15.0f, 12.0f, 10.0f, 8.0f, 0.0f, a1, a2) && a2 > 0.0f,
          "bendSign 0 should pick the elbow at positive a2 (a2 %g)", (double)a2);
  }

  // ---- bad lengths and targets: false, angles untouched ----
  {
    const float inf = INFINITY, nan = NAN;
    const float bad[][4] = {
        {0.0f, 12.0f, 10.0f, 5.0f},  {15.0f, 0.0f, 10.0f, 5.0f}, {-15.0f, 12.0f, 10.0f, 5.0f},
        {nan, 12.0f, 10.0f, 5.0f},   {15.0f, inf, 10.0f, 5.0f},  {15.0f, 12.0f, nan, 5.0f},
        {15.0f, 12.0f, 10.0f, -inf},
    };
    long touched = 0, accepted = 0;
    for (const auto &b : bad) {
      float a1 = 0.25f, a2 = 0.5f;
      if (sd_ik2(b[0], b[1], b[2], b[3], 1.0f, a1, a2)) ++accepted;
      if (a1 != 0.25f || a2 != 0.5f) ++touched;
    }
    CHECK(accepted == 0 && touched == 0, "bad inputs: %ld accepted, %ld changed the angles",
          accepted, touched);
  }

  // ---- the angles as a chain in a rig: the tip on the target ----
  {
    // the chain hangs from a shoulder joint, plain or mirrored, turned or
    // not; sd_ik2 works in the joint's frame, so the panel target goes
    // through the inverse of the joint's world transform first
    const float sx = 40.0f, sy = 30.0f, l1 = 15.0f, l2 = 12.0f;
    const float targets[][2] = {{20.0f, 5.0f}, {-8.0f, 17.0f}, {3.0f, -22.0f}, {-14.0f, -9.0f}};
    long bad = 0, cases = 0;
    for (int flip = 0; flip < 2; ++flip)
      for (int turn = 0; turn < 2; ++turn)
        for (int side = 0; side < 2; ++side)
          for (const auto &t : targets) {
            SD_Bone bones[4] = {
                {-1, sx, sy, turn ? 0.3f : 0.0f, flip != 0, true, nullptr},  // the shoulder
                {0, 0.0f, 0.0f, 0.0f, false, true, nullptr},                 // link 1
                {1, l1, 0.0f, 0.0f, false, true, nullptr},                   // link 2
                {2, l2, 0.0f, 0.0f, false, true, nullptr},                   // the tip
            };
            SD_Xform world[4];
            SD_Rig rig(bones, world, 4);
            ++cases;
            if (!rig.pose()) ++bad;
            float cx, cy;
            rig.world(0).inverse().apply(sx + t[0], sy + t[1], &cx, &cy);
            if (!sd_ik2(l1, l2, cx, cy, side ? -1.0f : 1.0f, bones[1].angle, bones[2].angle))
              ++bad;
            if (!rig.pose()) ++bad;
            float x, y;
            rig.world(3).apply(0.0f, 0.0f, &x, &y);
            if (fabsf(x - (sx + t[0])) > 1e-3f || fabsf(y - (sy + t[1])) > 1e-3f) ++bad;
            // an unturned mirrored joint's frame is the panel's with x negated
            if (flip && !turn && (fabsf(cx + t[0]) > 1e-5f || fabsf(cy - t[1]) > 1e-5f)) ++bad;
          }
    CHECK(bad == 0, "ik through a rig: %ld failures over %ld chains", bad, cases);
  }
}
