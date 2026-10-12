// SPDX-License-Identifier: MIT
// rig suite: a three-bone chain posed by hand matches pose(); a child
// before its parent (or its own parent, or out of range) is refused by
// sd_validateRig and by pose(), which leaves the bad bone and those after it
// unposed and undrawn; draw order is array order (a later bone's black
// paints over an earlier bone's white, and its white over black); a pure
// joint and an invisible bone are posed and never drawn, their children
// drawn; a mirrored parent carries its child where the mirror says (the
// child's ox flips, its angle turns the other way, a mirrored child under a
// mirrored parent is unmirrored, and the frame at angle 0 is the unmirrored
// frame reflected pixel for pixel); and golden frames of the stage 0 arm
// and mallet fixtures as a chain at pinned angles.

#include <math.h>
#include <string.h>

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Rig.h"

#include "test_common.h"
#include "test_fixtures.h"

static bool near(double a, double b, double tol) { return fabs(a - b) <= tol; }

// every entry of m within tol of n; the translations within ttol.
static bool nearXform(const SD_Xform &m, const SD_Xform &n, double tol, double ttol) {
  return near(m.a, n.a, tol) && near(m.b, n.b, tol) && near(m.c, n.c, tol) &&
         near(m.d, n.d, tol) && near(m.tx, n.tx, ttol) && near(m.ty, n.ty, ttol);
}

static bool isZero(const SD_Xform &m) {
  return m.a == 0.0f && m.b == 0.0f && m.c == 0.0f && m.d == 0.0f && m.tx == 0.0f &&
         m.ty == 0.0f;
}

static bool sameCanvas(const SD_Canvas &a, const SD_Canvas &b) {
  return a.bytes() == b.bytes() && memcmp(a.buffer(), b.buffer(), a.bytes()) == 0;
}

static void checker(SD_Canvas &c) {
  for (int16_t y = 0; y < c.height(); ++y)
    for (int16_t x = 0; x < c.width(); ++x) c.plot(x, y, ((x + y) & 1) != 0);
}

// 6x6 blocks, pivot at the top-left pixel: one all white, one all black.
static const uint8_t kBlockFull[6] = {0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc};
static const uint8_t kBlockNone[6] = {0, 0, 0, 0, 0, 0};
static const SD_Sprite kWhiteBlock = {6, 6, 0, 0, kBlockFull, kBlockFull};
static const SD_Sprite kBlackBlock = {6, 6, 0, 0, kBlockNone, kBlockFull};

// ---- rig goldens: the arm, a second arm pinned at its far end, the mallet
// pinned at that one's far end; each pose's root, elbow and wrist angles in
// degrees, the last pose the second mirrored at the root ----
static const int kRigPoseCount = 5;
static const int kRigPoseDeg[kRigPoseCount][3] = {
    {0, 0, 0}, {20, 45, -70}, {90, -90, 30}, {-135, 60, 45}, {20, 45, -70}};
static const bool kRigPoseFlip[kRigPoseCount] = {false, false, false, false, true};

// the rig golden canvas: 96x96, the root's pivot at (48, 48), drawn on a
// canvas cleared dark and on one cleared lit; the hash is fnv-1a over the
// dark canvas's bytes, then on through the lit canvas's.
static const int16_t kRigW = 96, kRigH = 96;

// pinned rig golden hashes, in pose, sampling (1x, 2x2) order. regenerate
// only deliberately: a moved pin is a changelog entry and a version bump.
static const uint64_t GOLDEN_RIG[kRigPoseCount][2] = {
    {0x82c60a3c9db2c34dULL, 0x82c60a3c9db2c34dULL},  // 0, 0, 0
    {0xed6e170d9de0b510ULL, 0x74a894d69bb55699ULL},  // 20, 45, -70
    {0x786c80b9e13b5ce5ULL, 0x7abc2132d192f63aULL},  // 90, -90, 30
    {0xdf86282abfcb6639ULL, 0x3bd536035ce0a276ULL},  // -135, 60, 45
    {0xcabb0631bece038fULL, 0x9989169591467b5bULL},  // 20, 45, -70, mirrored
};

static uint64_t rigHash(const int deg[3], bool flip, SD_Sample q, const char *label) {
  const SD_Bone bones[3] = {
      {-1, 48.0f, 48.0f, degToRad((float)deg[0]), flip, true, &kArm.sprite},
      {0, 15.0f, 0.0f, degToRad((float)deg[1]), false, true, &kArm.sprite},
      {1, 15.0f, 0.0f, degToRad((float)deg[2]), false, true, &kMallet.sprite},
  };
  SD_Xform world[3];
  SD_Rig rig(bones, world, 3);
  CHECK(rig.pose(), "%s: pose() refused a valid chain", label);
  OwnedCanvas dark(kRigW, kRigH), lit(kRigW, kRigH);
  dark.canvas.clear(false);
  lit.canvas.clear(true);
  rig.draw(dark.canvas, q);
  rig.draw(lit.canvas, q);
  char name[96];
  std::snprintf(name, sizeof(name), "%s_dark", label);
  writeFrame(dark.canvas, name);
  std::snprintf(name, sizeof(name), "%s_lit", label);
  writeFrame(lit.canvas, name);
  return fnv1a(lit.canvas.buffer(), lit.canvas.bytes(),
               fnv1a(dark.canvas.buffer(), dark.canvas.bytes()));
}

void runRigTests() {
  // ---- a three-bone chain posed by hand matches pose() ----
  {
    const SD_Bone bones[3] = {
        {-1, 20.0f, 15.0f, 0.3f, false, true, &kArm.sprite},
        {0, 10.0f, 2.0f, -0.5f, false, true, &kArm.sprite},
        {1, 7.0f, -1.0f, 1.1f, false, true, &kMallet.sprite},
    };
    SD_Xform world[3];
    SD_Rig rig(bones, world, 3);
    CHECK(sd_validateRig(bones, 3), "a parents-first chain should validate");
    CHECK(rig.pose(), "pose() refused a parents-first chain");
    // by hand: each pivot is its parent's pivot plus its offset turned by
    // the parent's accumulated angle; each angle is the parent's plus its own
    double px = 20.0, py = 15.0, ang = 0.3;
    long bad = 0;
    for (int i = 0; i < 3; ++i) {
      if (i > 0) {
        const double ox = bones[i].ox, oy = bones[i].oy;
        const double nx = px + cos(ang) * ox - sin(ang) * oy;
        const double ny = py + sin(ang) * ox + cos(ang) * oy;
        px = nx;
        py = ny;
        ang += bones[i].angle;
      }
      SD_Xform want;
      want.a = (float)cos(ang);
      want.b = (float)-sin(ang);
      want.c = (float)sin(ang);
      want.d = (float)cos(ang);
      want.tx = (float)px;
      want.ty = (float)py;
      if (!nearXform(rig.world(i), want, 1e-5, 1e-4)) ++bad;
      float x, y;
      rig.world(i).apply(0.0f, 0.0f, &x, &y);
      if (!near(x, px, 1e-4) || !near(y, py, 1e-4)) ++bad;
    }
    CHECK(bad == 0, "three-bone chain: pose() differs from the hand pose at %ld checks", bad);
    CHECK(isZero(rig.world(-1)) && isZero(rig.world(3)), "world() outside the rig is not zero");
  }

  // ---- a child before its parent, its own parent, or out of range: refused ----
  {
    struct Bad {
      int bone;
      int8_t parent;
      const char *what;
    };
    static const Bad bads[] = {
        {1, 2, "a child before its parent"},
        {1, 1, "a bone its own parent"},
        {2, 5, "a parent past the end"},
        {1, -2, "a parent below -1"},
        {0, 0, "a root with parent 0"},
    };
    for (const Bad &b : bads) {
      SD_Bone bones[3] = {
          {-1, 20.0f, 15.0f, 0.3f, false, true, &kArm.sprite},
          {0, 15.0f, 0.0f, -0.5f, false, true, &kArm.sprite},
          {1, 15.0f, 0.0f, 1.1f, false, true, &kMallet.sprite},
      };
      bones[b.bone].parent = b.parent;
      SD_Xform world[3];
      SD_Rig rig(bones, world, 3);
      CHECK(!sd_validateRig(bones, 3), "sd_validateRig accepted %s", b.what);
      CHECK(!rig.pose(), "pose() accepted %s", b.what);
      bool unposed = true;
      for (int i = b.bone; i < 3; ++i) unposed = unposed && isZero(rig.world(i));
      CHECK(unposed, "%s: the bad bone and those after it should be unposed", b.what);
      if (b.bone > 0)
        CHECK(nearXform(rig.world(0), SD_Xform::rigid(20.0f, 15.0f, 0.3f), 0, 0),
              "%s: the bones before the bad one should be posed", b.what);
      // drawn, only the posed bones show
      OwnedCanvas got(64, 48), want(64, 48);
      got.canvas.clear(false);
      want.canvas.clear(false);
      rig.draw(got.canvas);
      for (int i = 0; i < b.bone; ++i) sd_draw(want.canvas, *bones[i].sprite, rig.world(i));
      CHECK(sameCanvas(got.canvas, want.canvas), "%s: an unposed bone was drawn", b.what);
    }
    const SD_Bone one = {-1, 0.0f, 0.0f, 0.0f, false, true, nullptr};
    CHECK(sd_validateRig(nullptr, 0), "an empty rig should validate");
    CHECK(!sd_validateRig(nullptr, 2), "a null bone array accepted");
    CHECK(!sd_validateRig(&one, -1), "a negative count accepted");
    CHECK(sd_validateRig(&one, 1), "a lone root refused");
    SD_Xform w[2];
    SD_Rig noBones(nullptr, w, 2), noWorld(&one, nullptr, 1);
    OwnedCanvas oc(8, 8);
    noBones.draw(oc.canvas);
    noWorld.draw(oc.canvas);
    CHECK(!noBones.pose() && !noWorld.pose(), "a rig over a null array posed");
    CHECK(isZero(noBones.world(0)) && isZero(noWorld.world(0)),
          "a rig over a null array reads a posed transform");
  }

  // ---- a rig never posed draws nothing ----
  {
    const SD_Bone bones[1] = {{-1, 20.0f, 15.0f, 0.0f, false, true, &kArm.sprite}};
    SD_Xform world[1] = {SD_Xform::rigid(20.0f, 15.0f, 0.0f)};
    SD_Rig rig(bones, world, 1);
    OwnedCanvas oc(48, 32);
    oc.canvas.clear(false);
    rig.draw(oc.canvas);
    long lit = 0;
    for (size_t i = 0; i < oc.canvas.bytes(); ++i) lit += oc.canvas.buffer()[i] != 0;
    CHECK(lit == 0 && isZero(rig.world(0)), "a rig never posed drew (%ld bytes lit)", lit);
  }

  // ---- draw order is array order ----
  for (int order = 0; order < 2; ++order) {
    const SD_Sprite *first = order == 0 ? &kWhiteBlock : &kBlackBlock;
    const SD_Sprite *second = order == 0 ? &kBlackBlock : &kWhiteBlock;
    const SD_Bone bones[2] = {
        {-1, 10.0f, 10.0f, 0.0f, false, true, first},
        {-1, 13.0f, 13.0f, 0.0f, false, true, second},
    };
    SD_Xform world[2];
    SD_Rig rig(bones, world, 2);
    rig.pose();
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      OwnedCanvas oc(24, 24);
      checker(oc.canvas);
      rig.draw(oc.canvas, q);
      long bad = 0;
      const bool firstLit = order == 0, secondLit = !firstLit;
      for (int16_t y = 0; y < 24; ++y)
        for (int16_t x = 0; x < 24; ++x) {
          const bool inFirst = x >= 10 && x < 16 && y >= 10 && y < 16;
          const bool inSecond = x >= 13 && x < 19 && y >= 13 && y < 19;
          bool want = ((x + y) & 1) != 0;
          if (inSecond)
            want = secondLit;  // the later bone paints over the earlier
          else if (inFirst)
            want = firstLit;
          if (oc.canvas.pixel(x, y) != want) ++bad;
        }
      CHECK(bad == 0, "draw order (%s second, %s): %ld pixels wrong",
            order == 0 ? "black" : "white", q == SD_SAMPLE_1 ? "1x" : "2x2", bad);
    }
  }

  // ---- a pure joint is posed and not drawn; an invisible bone the same ----
  for (int kind = 0; kind < 2; ++kind) {
    // kind 0: a joint with no sprite; kind 1: an arm marked invisible
    const SD_Bone bones[2] = {
        {-1, 30.0f, 20.0f, 0.4f, false, kind == 0, kind == 0 ? nullptr : &kArm.sprite},
        {0, 15.0f, 0.0f, 0.2f, false, true, &kMallet.sprite},
    };
    const char *what = kind == 0 ? "pure joint" : "invisible bone";
    SD_Xform world[2];
    SD_Rig rig(bones, world, 2);
    CHECK(rig.pose(), "%s: pose() refused", what);
    CHECK(nearXform(rig.world(0), SD_Xform::rigid(30.0f, 20.0f, 0.4f), 0, 0),
          "%s: not posed where its bone says", what);
    const SD_Xform child = SD_Xform::compose(SD_Xform::rigid(30.0f, 20.0f, 0.4f),
                                             SD_Xform::rigid(15.0f, 0.0f, 0.2f));
    CHECK(nearXform(rig.world(1), child, 0, 0), "%s: its child is not posed under it", what);
    OwnedCanvas got(64, 48), want(64, 48);
    checker(got.canvas);
    checker(want.canvas);
    rig.draw(got.canvas);
    sd_draw(want.canvas, kMallet.sprite, child);
    CHECK(sameCanvas(got.canvas, want.canvas),
          "%s: drawn, or its child not drawn, differs from the child alone", what);
  }

  // ---- a mirrored parent carries its child where the mirror says ----
  {
    // at angle 0: the child's ox flips, its angle turns the other way
    const SD_Bone bones[2] = {
        {-1, 20.0f, 20.0f, 0.0f, true, true, &kArm.sprite},
        {0, 6.0f, 2.0f, 0.4f, false, true, &kMallet.sprite},
    };
    SD_Xform world[2];
    SD_Rig rig(bones, world, 2);
    rig.pose();
    float x, y;
    rig.world(1).apply(0.0f, 0.0f, &x, &y);
    CHECK(x == 14.0f && y == 22.0f, "mirrored parent: child's pivot at (%g, %g), want (14, 22)",
          x, y);
    CHECK(nearXform(rig.world(1), SD_Xform::rigid(14.0f, 22.0f, -0.4f, true), 1e-6, 1e-6),
          "mirrored parent: the child should be rigid(14, 22, -0.4, mirrored)");
  }
  {
    // turned: rigid(p, 0.7, mirrored) then rigid(o, 0.4) is rigid(p + o
    // mirrored and turned, 0.7 - 0.4, mirrored); a mirrored child under it
    // is unmirrored at the same angle
    for (int childFlip = 0; childFlip < 2; ++childFlip) {
      const SD_Bone bones[2] = {
          {-1, 20.0f, 20.0f, 0.7f, true, true, &kArm.sprite},
          {0, 6.0f, 2.0f, 0.4f, childFlip != 0, true, &kMallet.sprite},
      };
      SD_Xform world[2];
      SD_Rig rig(bones, world, 2);
      rig.pose();
      const double wx = 20.0 + cos(0.7) * -6.0 - sin(0.7) * 2.0;
      const double wy = 20.0 + sin(0.7) * -6.0 + cos(0.7) * 2.0;
      const SD_Xform want = SD_Xform::rigid((float)wx, (float)wy, 0.7f - 0.4f, childFlip == 0);
      CHECK(nearXform(rig.world(1), want, 1e-6, 1e-5),
            "mirrored parent at 0.7 rad, child %s: not rigid((%g, %g), 0.3, %s)",
            childFlip ? "mirrored" : "unmirrored", wx, wy, childFlip ? "unmirrored" : "mirrored");
    }
  }
  {
    // pixel for pixel at angle 0: the mirrored chain is the chain reflected
    // about the root's column (x = 40 on an 81-wide checker, which the
    // reflection keeps)
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      OwnedCanvas plain(81, 24), mirrored(81, 24);
      checker(plain.canvas);
      checker(mirrored.canvas);
      for (int flip = 0; flip < 2; ++flip) {
        const SD_Bone bones[2] = {
            {-1, 40.0f, 12.0f, 0.0f, flip != 0, true, &kArm.sprite},
            {0, 15.0f, 0.0f, 0.0f, false, true, &kFlag.sprite},
        };
        SD_Xform world[2];
        SD_Rig rig(bones, world, 2);
        rig.pose();
        rig.draw(flip ? mirrored.canvas : plain.canvas, q);
      }
      long bad = 0;
      for (int16_t yy = 0; yy < 24; ++yy)
        for (int16_t xx = 0; xx < 81; ++xx)
          if (mirrored.canvas.pixel(xx, yy) != plain.canvas.pixel((int16_t)(80 - xx), yy)) ++bad;
      CHECK(bad == 0, "mirrored chain (%s): %ld pixels differ from the chain reflected",
            q == SD_SAMPLE_1 ? "1x" : "2x2", bad);
    }
  }

  // ---- goldens: the arm, arm and mallet chain at the pinned poses ----
  for (int p = 0; p < kRigPoseCount; ++p)
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      char label[64];
      std::snprintf(label, sizeof(label), "rig_%d_%d_%d%s_%s", kRigPoseDeg[p][0],
                    kRigPoseDeg[p][1], kRigPoseDeg[p][2], kRigPoseFlip[p] ? "_mirrored" : "",
                    qi == 0 ? "1x" : "2x2");
      const uint64_t h = rigHash(kRigPoseDeg[p], kRigPoseFlip[p], q, label);
      CHECK(h == GOLDEN_RIG[p][qi], "%s: hash 0x%016llxULL != pinned 0x%016llxULL", label,
            (unsigned long long)h, (unsigned long long)GOLDEN_RIG[p][qi]);
    }
}
