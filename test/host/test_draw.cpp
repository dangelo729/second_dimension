// SPDX-License-Identifier: MIT
// draw suite: the canvas's layout; identity (angle 0 at an integer position,
// 1x and 2x2, is the art pixel for pixel and equals a drawBitmap-style
// copy); golden frames of every fixture at the pinned poses; the scratch
// mockup's frame; clipping (half off each edge, wholly off, a 1x1 target,
// and a fuzz against a brute-force pass over the whole target, all through a
// target that counts out-of-bounds plots, over exactly-sized heap planes);
// and the refusals (a bad sprite, a matrix with no finite inverse).

#include <math.h>
#include <string.h>

#include <vector>

#include "SD_Canvas.h"
#include "SD_Draw.h"

#include "test_common.h"
#include "test_fixtures.h"

// a target that counts every plot and every plot outside its bounds, and
// passes the in-bounds ones to a canvas.
class TrapTarget : public SD_Target {
 public:
  explicit TrapTarget(SD_Canvas &c) : c_(c) {}
  int16_t width() const override { return c_.width(); }
  int16_t height() const override { return c_.height(); }
  void plot(int16_t x, int16_t y, bool on) override {
    ++plots;
    if (x < 0 || y < 0 || x >= c_.width() || y >= c_.height()) {
      ++oob;
      return;
    }
    c_.plot(x, y, on);
  }
  long plots = 0;
  long oob = 0;

 private:
  SD_Canvas &c_;
};

static void checker(SD_Canvas &c) {
  for (int16_t y = 0; y < c.height(); ++y)
    for (int16_t x = 0; x < c.width(); ++x) c.plot(x, y, ((x + y) & 1) != 0);
}

static const char *sampleName(SD_Sample q) { return q == SD_SAMPLE_1 ? "1x" : "2x2"; }

// sd_draw's rule applied to every pixel of the target, with no box: the
// inverse from SD_Xform::inverse, the pivot added to its translation, each
// sample carried by apply and rounded by floorf(u + 0.5f).
static void bruteForce(SD_Canvas &c, const SD_Sprite &s, const SD_Xform &m, SD_Sample q) {
  static const float p1[1][2] = {{0.0f, 0.0f}};
  static const float p4[4][2] = {
      {-0.25f, -0.25f}, {0.25f, -0.25f}, {-0.25f, 0.25f}, {0.25f, 0.25f}};
  const float(*pts)[2] = q == SD_SAMPLE_1 ? p1 : p4;
  const int n = q == SD_SAMPLE_1 ? 1 : 4;
  SD_Xform inv = m.inverse();
  inv.tx = inv.tx + (float)s.pivotX;
  inv.ty = inv.ty + (float)s.pivotY;
  for (int16_t py = 0; py < c.height(); ++py)
    for (int16_t px = 0; px < c.width(); ++px) {
      bool hit = false, lit = false;
      for (int k = 0; k < n; ++k) {
        float u, v;
        inv.apply((float)px + pts[k][0], (float)py + pts[k][1], &u, &v);
        const float fu = floorf(u + 0.5f), fv = floorf(v + 0.5f);
        if (!(fu >= 0.0f && fu < (float)s.w && fv >= 0.0f && fv < (float)s.h)) continue;
        bool ink;
        if (sd_spritePixel(s, (int16_t)fu, (int16_t)fv, ink)) {
          hit = true;
          lit = lit || ink;
        }
      }
      if (hit) c.plot(px, py, lit);
    }
}

// ---- golden poses: each fixture at these angles, the last one mirrored ----
static const int kPoseCount = 7;
static const int kPoseDeg[kPoseCount] = {0, 20, 45, 70, 90, 180, 20};
static const bool kPoseFlip[kPoseCount] = {false, false, false, false, false, false, true};

// the golden canvas: 40x40, the sprite's pivot at (20, 20), drawn twice, once
// on a canvas cleared dark and once on one cleared lit (so its black pixels
// and its unmasked ones both show); the hash is fnv-1a over the dark
// canvas's bytes, then on through the lit canvas's.
static const int16_t kGoldW = 40, kGoldH = 40;
static const float kGoldX = 20.0f, kGoldY = 20.0f;

static uint64_t goldenHash(const SD_Sprite &s, int deg, bool flip, SD_Sample q,
                           const char *label) {
  OwnedCanvas dark(kGoldW, kGoldH), lit(kGoldW, kGoldH);
  dark.canvas.clear(false);
  lit.canvas.clear(true);
  const SD_Xform m = SD_Xform::rigid(kGoldX, kGoldY, degToRad((float)deg), flip);
  TrapTarget td(dark.canvas), tl(lit.canvas);
  const bool ok1 = sd_draw(td, s, m, q);
  const bool ok2 = sd_draw(tl, s, m, q);
  CHECK(ok1 && ok2, "%s: sd_draw returned false", label);
  CHECK(td.oob == 0 && tl.oob == 0, "%s: plotted out of bounds", label);
  char name[96];
  std::snprintf(name, sizeof(name), "%s_dark", label);
  writeFrame(dark.canvas, name);
  std::snprintf(name, sizeof(name), "%s_lit", label);
  writeFrame(lit.canvas, name);
  return fnv1a(lit.canvas.buffer(), lit.canvas.bytes(),
               fnv1a(dark.canvas.buffer(), dark.canvas.bytes()));
}

// pinned golden hashes, in fixture, pose, sampling order. each entry names
// its fixture, angle in degrees (clockwise on the panel), mirroring and
// sampling; all are on the 40x40 dark-then-lit canvas pair above. regenerate
// only deliberately: a moved pin is a changelog entry and a version bump.
static const uint64_t GOLDEN_POSE[kFixtureCount][kPoseCount][2] = {
    {
        // arm, 16x7, pivot (0, 3)
        {0xee567d64a350b5ffULL, 0xee567d64a350b5ffULL},  // 0 deg: 1x, 2x2
        {0x6d872b8bd8f7c09dULL, 0x5db31a16a171389aULL},  // 20 deg
        {0xfae1624100f410daULL, 0x2289423af6f655f4ULL},  // 45 deg
        {0xd9d4beec5072467eULL, 0x120916b59b4f5a80ULL},  // 70 deg
        {0x27b699199dad9ebfULL, 0x27b699199dad9ebfULL},  // 90 deg
        {0x068192e4e05c3147ULL, 0x068192e4e05c3147ULL},  // 180 deg
        {0xd85c7620119271c2ULL, 0xa9d3aa0fdbe7955aULL},  // 20 deg, mirrored
    },
    {
        // mallet, 12x5, pivot (0, 2)
        {0x37431bdbf3b54510ULL, 0x37431bdbf3b54510ULL},  // 0 deg: 1x, 2x2
        {0xb579c8ecd63773ceULL, 0x2185341d4f9c1a0eULL},  // 20 deg
        {0x7f8d43eb2dbfe07aULL, 0x9807ff4c9ac3b2aaULL},  // 45 deg
        {0x5ee4a1a7986cef0aULL, 0x5b678f53e0a56b50ULL},  // 70 deg
        {0xdc40746f3f6c7f8dULL, 0xdc40746f3f6c7f8dULL},  // 90 deg
        {0x8994fbf8b16163d2ULL, 0x8994fbf8b16163d2ULL},  // 180 deg
        {0x51e5bc1b6f12e132ULL, 0x3ec67ad4e1663a01ULL},  // 20 deg, mirrored
    },
    {
        // flag, 9x11, pivot (0, 10)
        {0x593c44a713c4185aULL, 0x593c44a713c4185aULL},  // 0 deg: 1x, 2x2
        {0xa28f9769b57421efULL, 0xcbf9c6fe415e4ab5ULL},  // 20 deg
        {0xb61ea3d23e9dd64fULL, 0xe00c01db563cc87fULL},  // 45 deg
        {0x0804e8c307d0734eULL, 0x7bdbf004c39dec94ULL},  // 70 deg
        {0x00b9a00a8a5e1b88ULL, 0x00b9a00a8a5e1b88ULL},  // 90 deg
        {0x82895f7b04457034ULL, 0x82895f7b04457034ULL},  // 180 deg
        {0xabfce2339ce28c4fULL, 0xb3dca6a2c371ac91ULL},  // 20 deg, mirrored
    },
};

void runDrawTests() {
  // ---- canvas: drawBitmap's row layout, read-back, bounds ----
  {
    OwnedCanvas oc(12, 3);
    SD_Canvas &c = oc.canvas;
    CHECK(c.stride() == 2 && c.bytes() == 6, "12x3 canvas: stride %d bytes %zu", c.stride(),
          c.bytes());
    c.clear(false);
    c.plot(0, 0, true);
    c.plot(9, 1, true);
    c.plot(11, 2, true);
    const uint8_t *b = c.buffer();
    CHECK(b[0] == 0x80 && b[1] == 0 && b[2] == 0 && b[3] == 0x40 && b[4] == 0 && b[5] == 0x10,
          "canvas layout: bytes %02x %02x %02x %02x %02x %02x", b[0], b[1], b[2], b[3], b[4],
          b[5]);
    CHECK(c.pixel(0, 0) && c.pixel(9, 1) && c.pixel(11, 2) && !c.pixel(1, 0),
          "canvas read-back disagrees with plot");
    c.plot(9, 1, false);
    CHECK(b[3] == 0, "dark plot did not clear its bit");
    c.plot(-1, 0, true);
    c.plot(12, 0, true);
    c.plot(0, 3, true);
    c.plot(0, -1, true);
    CHECK(b[0] == 0x80 && b[2] == 0 && b[4] == 0 && b[5] == 0x10,
          "out-of-range plots changed the buffer");
    CHECK(!c.pixel(-1, 0) && !c.pixel(12, 0) && !c.pixel(0, 3), "out-of-range pixel reads lit");
    c.clear(true);
    long ones = 0;
    for (size_t i = 0; i < c.bytes(); ++i) ones += b[i] == 0xff;
    CHECK(ones == 6, "clear(true) left %ld of 6 bytes unfilled", 6 - ones);
    SD_Canvas neg(nullptr, -4, -4);
    CHECK(neg.width() == 0 && neg.height() == 0 && neg.bytes() == 0,
          "negative canvas size not taken as zero");
  }

  // ---- identity: angle 0 at an integer position is the art, and the gfx copy ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    const SD_Sprite &s = fx.sprite;
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      const int16_t W = 32, H = 24, X = 9, Y = 12;
      OwnedCanvas got(W, H), want(W, H);
      checker(got.canvas);
      checker(want.canvas);
      TrapTarget t(got.canvas);
      CHECK(sd_draw(t, s, SD_Xform::rigid(X, Y, 0.0f), q), "%s identity: sd_draw false",
            fx.name);
      referenceDraw(want.canvas, s, X, Y);
      long bad = 0;
      for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x) {
          const char a = fx.at(x - X + s.pivotX, y - Y + s.pivotY);
          const bool under = ((x + y) & 1) != 0;
          const bool expect = a == '#' ? true : a == '-' ? false : under;
          if (got.canvas.pixel(x, y) != expect) ++bad;
        }
      CHECK(bad == 0, "%s identity %s: %ld pixels differ from the art", fx.name, sampleName(q),
            bad);
      CHECK(memcmp(got.canvas.buffer(), want.canvas.buffer(), got.canvas.bytes()) == 0,
            "%s identity %s: differs from the drawBitmap-style copy", fx.name, sampleName(q));
      CHECK(t.oob == 0, "%s identity: plotted out of bounds", fx.name);

      // mirrored at angle 0: the art reflected about the pivot's column
      OwnedCanvas mir(W, H);
      checker(mir.canvas);
      sd_draw(mir.canvas, s, SD_Xform::rigid(X, Y, 0.0f, true), q);
      bad = 0;
      for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x) {
          const char a = fx.at(s.pivotX - (x - X), y - Y + s.pivotY);
          const bool under = ((x + y) & 1) != 0;
          const bool expect = a == '#' ? true : a == '-' ? false : under;
          if (mir.canvas.pixel(x, y) != expect) ++bad;
        }
      CHECK(bad == 0, "%s mirrored identity %s: %ld pixels differ from the reflected art",
            fx.name, sampleName(q), bad);
    }
  }

  // ---- quarter turns lose nothing: 90, 180 and 270 degrees are the art turned ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    const SD_Sprite &s = fx.sprite;
    for (int turn = 1; turn < 4; ++turn)
      for (int qi = 0; qi < 2; ++qi) {
        const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        const int16_t W = 40, H = 40, X = 20, Y = 20;
        OwnedCanvas oc(W, H);
        checker(oc.canvas);
        sd_draw(oc.canvas, s, SD_Xform::rigid(X, Y, degToRad(90.0f * turn)), q);
        long bad = 0;
        for (int16_t y = 0; y < H; ++y)
          for (int16_t x = 0; x < W; ++x) {
            // undo the turn: a quarter turn clockwise takes (dx, dy) to (-dy, dx)
            int dx = x - X, dy = y - Y;
            for (int k = 0; k < turn; ++k) {
              const int t = dx;
              dx = dy;
              dy = -t;
            }
            const char a = fx.at(dx + s.pivotX, dy + s.pivotY);
            const bool expect = a == '#' ? true : a == '-' ? false : ((x + y) & 1) != 0;
            if (oc.canvas.pixel(x, y) != expect) ++bad;
          }
        CHECK(bad == 0, "%s at %d degrees (%s): %ld pixels differ from the art turned", fx.name,
              90 * turn, sampleName(q), bad);
      }
  }

  // ---- goldens: every fixture at every pose, both samplings ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    for (int p = 0; p < kPoseCount; ++p) {
      for (int qi = 0; qi < 2; ++qi) {
        const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        char label[64];
        std::snprintf(label, sizeof(label), "golden_%s_%03d%s_%s", fx.name, kPoseDeg[p],
                      kPoseFlip[p] ? "_mirrored" : "", sampleName(q));
        const uint64_t h = goldenHash(fx.sprite, kPoseDeg[p], kPoseFlip[p], q, label);
        CHECK(h == GOLDEN_POSE[f][p][qi], "%s: hash 0x%016llxULL != pinned 0x%016llxULL", label,
              (unsigned long long)h, (unsigned long long)GOLDEN_POSE[f][p][qi]);
      }
    }
  }

  // ---- the scratch mockup's frame ----
  {
    OwnedCanvas oc(kMockupW, kMockupH);
    oc.canvas.clear(false);
    drawMockup(oc.canvas, kArm.sprite, &kMallet.sprite);
    const uint64_t h = canvasHash(oc.canvas, "mockup");
    CHECK(h == GOLDEN_MOCKUP, "mockup: hash 0x%016llxULL != pinned 0x%016llxULL",
          (unsigned long long)h, (unsigned long long)GOLDEN_MOCKUP);
  }

  // ---- clipping at angle 0: half off each edge is the visible half of the art ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    HeapSprite hs(fx.sprite);
    const SD_Sprite &s = hs.sprite;
    const int16_t W = 20, H = 16;
    // pivot positions that put the sprite's middle on each edge
    const int16_t midX = (int16_t)(s.w / 2 - s.pivotX), midY = (int16_t)(s.h / 2 - s.pivotY);
    const int16_t pos[4][2] = {
        {(int16_t)(0 - midX), (int16_t)(H / 2)},
        {(int16_t)(W - 1 - midX), (int16_t)(H / 2)},
        {(int16_t)(W / 2), (int16_t)(0 - midY)},
        {(int16_t)(W / 2), (int16_t)(H - 1 - midY)},
    };
    static const char *edge[4] = {"left", "right", "top", "bottom"};
    for (int e = 0; e < 4; ++e)
      for (int qi = 0; qi < 2; ++qi) {
        const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        OwnedCanvas oc(W, H);
        checker(oc.canvas);
        TrapTarget t(oc.canvas);
        const int16_t X = pos[e][0], Y = pos[e][1];
        sd_draw(t, s, SD_Xform::rigid(X, Y, 0.0f), q);
        long bad = 0;
        for (int16_t y = 0; y < H; ++y)
          for (int16_t x = 0; x < W; ++x) {
            const char a = fx.at(x - X + s.pivotX, y - Y + s.pivotY);
            const bool expect = a == '#' ? true : a == '-' ? false : ((x + y) & 1) != 0;
            if (oc.canvas.pixel(x, y) != expect) ++bad;
          }
        CHECK(bad == 0, "%s half off the %s edge (%s): %ld pixels wrong", fx.name, edge[e],
              sampleName(q), bad);
        CHECK(t.oob == 0, "%s half off the %s edge: %ld plots out of bounds", fx.name, edge[e],
              t.oob);
        CHECK(t.plots > 0, "%s half off the %s edge: nothing plotted", fx.name, edge[e]);
      }
  }

  // ---- clipping, turned: half off each edge at 45 degrees ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    HeapSprite hs(fx.sprite);
    const SD_Sprite &s = hs.sprite;
    const int16_t W = 20, H = 16;
    const float pos[4][2] = {{-4.0f, 8.0f}, {17.0f, 8.0f}, {10.0f, -3.0f}, {10.0f, 14.0f}};
    for (int e = 0; e < 4; ++e)
      for (int qi = 0; qi < 2; ++qi) {
        const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        const SD_Xform m = SD_Xform::rigid(pos[e][0], pos[e][1], degToRad(45.0f), e == 2);
        OwnedCanvas got(W, H), want(W, H);
        checker(got.canvas);
        checker(want.canvas);
        TrapTarget t(got.canvas);
        sd_draw(t, s, m, q);
        bruteForce(want.canvas, s, m, q);
        CHECK(memcmp(got.canvas.buffer(), want.canvas.buffer(), got.canvas.bytes()) == 0,
              "%s turned off edge %d (%s): differs from the brute-force pass", fx.name, e,
              sampleName(q));
        CHECK(t.oob == 0, "%s turned off edge %d: %ld plots out of bounds", fx.name, e, t.oob);
      }
  }

  // ---- clipping: wholly off every side, and far away ----
  {
    HeapSprite hs(kArm.sprite);
    const float far[][2] = {{-40.0f, 8.0f},  {60.0f, 8.0f},   {10.0f, -40.0f}, {10.0f, 60.0f},
                            {-1e6f, -1e6f},  {1e6f, 1e6f},    {-3e38f, 5.0f},  {5.0f, 3e38f},
                            {-40.0f, -40.0f}, {60.0f, 60.0f}};
    long plotted = 0, oob = 0, refused = 0;
    for (const auto &p : far)
      for (int qi = 0; qi < 2; ++qi)
        for (int a = 0; a < 3; ++a) {
          OwnedCanvas oc(20, 16);
          TrapTarget t(oc.canvas);
          if (!sd_draw(t, hs.sprite, SD_Xform::rigid(p[0], p[1], degToRad(a * 37.0f)),
                       qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2))
            ++refused;
          plotted += t.plots;
          oob += t.oob;
        }
    CHECK(plotted == 0, "a sprite wholly off the target plotted %ld pixels", plotted);
    CHECK(oob == 0, "wholly off: %ld plots out of bounds", oob);
    CHECK(refused == 0, "wholly off: sd_draw returned false %ld times (nothing is wrong)",
          refused);
  }

  // ---- clipping: a 1x1 target ----
  {
    HeapSprite hs(kArm.sprite);
    OwnedCanvas oc(1, 1);
    TrapTarget t(oc.canvas);
    oc.canvas.clear(false);
    sd_draw(t, hs.sprite, SD_Xform::rigid(0.0f, 0.0f, 0.0f), SD_SAMPLE_2x2);
    CHECK(oc.canvas.pixel(0, 0), "1x1: the arm's shoulder pixel should light the only pixel");
    CHECK(t.plots == 1, "1x1: %ld plots, want 1", t.plots);
    oc.canvas.clear(true);
    sd_draw(t, hs.sprite, SD_Xform::rigid(-1.0f, 0.0f, 0.0f), SD_SAMPLE_1);
    CHECK(oc.canvas.pixel(0, 0) == true, "1x1: the arm's unmasked inside should leave it lit");
    long oob = 0;
    for (int k = 0; k < 300; ++k) {
      TrapTarget tk(oc.canvas);
      const SD_Xform m = SD_Xform::rigid(frand(-20.0f, 20.0f), frand(-20.0f, 20.0f),
                                         frand(-7.0f, 7.0f), (k & 1) != 0);
      sd_draw(tk, hs.sprite, m, (k & 2) ? SD_SAMPLE_1 : SD_SAMPLE_2x2);
      oob += tk.oob;
    }
    CHECK(oob == 0, "1x1 target: %ld plots out of bounds", oob);
  }

  // ---- clipping fuzz: any transform equals the brute-force pass over the whole target ----
  {
    long mismatches = 0, oob = 0, refused = 0, drawn = 0;
    for (int k = 0; k < 1500; ++k) {
      const ArtFixture &fx = *kFixtures[k % kFixtureCount];
      HeapSprite hs(fx.sprite);
      const int16_t W = (int16_t)(1 + (k * 7) % 48), H = (int16_t)(1 + (k * 13) % 40);
      SD_Xform m;
      if (k % 3 != 2) {
        m = SD_Xform::rigid(frand(-20.0f, W + 20.0f), frand(-20.0f, H + 20.0f),
                            frand(-7.0f, 7.0f), (k & 4) != 0);
      } else {
        // scale and shear: the blit takes any affine
        do {
          m.a = frand(-2.5f, 2.5f);
          m.b = frand(-2.5f, 2.5f);
          m.c = frand(-2.5f, 2.5f);
          m.d = frand(-2.5f, 2.5f);
        } while (fabsf(m.a * m.d - m.b * m.c) < 0.1f);
        m.tx = frand(-20.0f, W + 20.0f);
        m.ty = frand(-20.0f, H + 20.0f);
      }
      const SD_Sample q = (k & 1) ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      OwnedCanvas got(W, H), want(W, H);
      checker(got.canvas);
      checker(want.canvas);
      TrapTarget t(got.canvas);
      if (!sd_draw(t, hs.sprite, m, q)) ++refused;
      bruteForce(want.canvas, hs.sprite, m, q);
      if (memcmp(got.canvas.buffer(), want.canvas.buffer(), got.canvas.bytes()) != 0) ++mismatches;
      oob += t.oob;
      drawn += t.plots > 0;
    }
    CHECK(mismatches == 0, "fuzz: %ld of 1500 draws differ from the brute-force pass",
          mismatches);
    CHECK(oob == 0, "fuzz: %ld plots out of bounds", oob);
    CHECK(refused == 0, "fuzz: %ld draws refused", refused);
    CHECK(drawn > 300, "fuzz: only %ld of 1500 draws plotted anything", drawn);
  }

  // ---- refusals: nothing is plotted and sd_draw returns false ----
  {
    OwnedCanvas oc(20, 16);
    TrapTarget t(oc.canvas);
    SD_Sprite bad = kArm.sprite;
    bad.mask = nullptr;
    CHECK(!sd_draw(t, bad, SD_Xform::rigid(5, 5, 0)), "null mask plane drawn");
    bad = kArm.sprite;
    bad.w = 0;
    CHECK(!sd_draw(t, bad, SD_Xform::rigid(5, 5, 0)), "zero-width sprite drawn");
    bad = kArm.sprite;
    bad.pivotY = 7;
    CHECK(!sd_draw(t, bad, SD_Xform::rigid(5, 5, 0)), "pivot outside the sprite drawn");
    const SD_Xform zero = {0, 0, 0, 0, 5, 5};
    CHECK(!sd_draw(t, kArm.sprite, zero), "zero matrix drawn");
    const SD_Xform flat = {1, 2, 2, 4, 5, 5};
    CHECK(!sd_draw(t, kArm.sprite, flat), "singular matrix drawn");
    const SD_Xform nan = {NAN, 0, 0, 1, 5, 5};
    CHECK(!sd_draw(t, kArm.sprite, nan), "nan matrix drawn");
    const SD_Xform inf = {1, 0, 0, 1, INFINITY, 5};
    CHECK(!sd_draw(t, kArm.sprite, inf), "infinite translation drawn");
    CHECK(t.plots == 0, "refused draws plotted %ld pixels", t.plots);
  }
}
