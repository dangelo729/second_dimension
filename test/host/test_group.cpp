// SPDX-License-Identifier: MIT
// group suite: sd_drawSilhouette and sd_drawInk are sd_draw's own pixels
// (every plot, and the lit ones) for every fixture at the golden poses; a
// lone rectangle drawn as a group is a 1 px outline with a dark inside; two
// overlapping rectangles give one contour and no line inside; interior ink
// shows; a shape touching the canvas's edge is outlined at the edge; a
// pinhole is filled and a wider hole outlined; nothing outside the shape is
// touched, on a dark, a lit and a checker background; the stamp never plots outside the target; the
// refusals; golden frames of a two-segment arm with a round elbow disc at
// pinned poses; and that arm through a root of scale 0.9 and 1.1, one shape
// with one 1 px outline at every pose (the outline is derived after the
// scale), pinned, and a piece of scale 0 left out of its group, even one
// posed onto (0, 0).

#include <math.h>
#include <string.h>

#include <vector>

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Group.h"
#include "SD_Rig.h"

#include "test_common.h"
#include "test_fixtures.h"

namespace {

// a target that remembers each pixel's last plot (0 never, 1 dark, 2 lit),
// counts plots outside it, and passes the rest to a canvas.
class Record : public SD_Target {
 public:
  explicit Record(SD_Canvas &c) : c_(c), at_((size_t)c.width() * (size_t)c.height(), 0) {}
  int16_t width() const override { return c_.width(); }
  int16_t height() const override { return c_.height(); }
  void plot(int16_t x, int16_t y, bool on) override {
    if (x < 0 || y < 0 || x >= c_.width() || y >= c_.height()) {
      ++oob;
      return;
    }
    at_[(size_t)y * (size_t)c_.width() + (size_t)x] = on ? 2 : 1;
    c_.plot(x, y, on);
  }
  uint8_t at(int x, int y) const { return at_[(size_t)y * (size_t)c_.width() + (size_t)x]; }
  long oob = 0;

 private:
  SD_Canvas &c_;
  std::vector<uint8_t> at_;
};

void checker(SD_Canvas &c) {
  for (int16_t y = 0; y < c.height(); ++y)
    for (int16_t x = 0; x < c.width(); ++x) c.plot(x, y, ((x + y) & 1) != 0);
}

// group pieces: solid shapes, '-' masked dark, '#' interior ink.
const ArtFixture kBlock("block",
                        {
                            "--------",
                            "--------",
                            "--------",
                            "--------",
                            "--------",
                        },
                        0, 2);

const ArtFixture kBlockInk("block_ink",
                           {
                               "--------",
                               "--------",
                               "--####--",
                               "--------",
                               "--------",
                           },
                           0, 2);

// a segment with round ends, its pivot in its left end's middle (2, 2) and
// its far pin 9 px along (11, 2); a round disc for the joint between two.
const ArtFixture kSeg("segment",
                      {
                          "..----------..",
                          ".------------.",
                          "--------------",
                          ".------------.",
                          "..----------..",
                      },
                      2, 2);

const ArtFixture kDisc("disc",
                       {
                           ".---.",
                           "-----",
                           "--#--",
                           "-----",
                           ".---.",
                       },
                       2, 2);

// a block with a pinhole (one unset pixel, its four neighbours set) and one
// with a two-pixel hole
const ArtFixture kPinhole("pinhole",
                          {
                              "-----",
                              "--.--",
                              "-----",
                          },
                          0, 1);

const ArtFixture kHole2("hole2",
                        {
                            "------",
                            "--..--",
                            "------",
                        },
                        0, 1);

// in the shape the stamp outlines: set, or a pinhole
bool inShape(const SD_Canvas &c, int x, int y) {
  const int16_t px = (int16_t)x, py = (int16_t)y;
  return c.pixel(px, py) || (c.pixel((int16_t)(x - 1), py) && c.pixel((int16_t)(x + 1), py) &&
                             c.pixel(px, (int16_t)(y - 1)) && c.pixel(px, (int16_t)(y + 1)));
}

// one bone at (x, y), angle deg, as a rig of one, drawn as a group of one
bool groupOfOne(SD_Target &t, const SD_Sprite &s, float x, float y, float deg, SD_Canvas &sil,
                SD_Canvas &ink, SD_Sample q = SD_SAMPLE_1) {
  const SD_Bone bones[1] = {{-1, x, y, degToRad(deg), false, false, &s}};
  SD_Xform world[1];
  SD_Rig rig(bones, world, 1);
  rig.pose();
  const int list[1] = {0};
  return sd_drawGroup(t, rig, list, 1, sil, ink, q);
}

// the arm: a segment at the shoulder, the disc on its far pin, a second
// segment from there; the group draws all three.
const int kArmW = 64, kArmH = 64;
const float kArmX = 22.0f, kArmY = 32.0f;

void armBones(SD_Bone (&b)[3], int a1, int a2, bool flip) {
  b[0] = {-1, kArmX, kArmY, degToRad((float)a1), flip, false, &kSeg.sprite};
  b[1] = {0, 9.0f, 0.0f, degToRad((float)-a1), false, false, &kDisc.sprite};
  b[2] = {0, 9.0f, 0.0f, degToRad((float)a2), false, false, &kSeg.sprite};
}

const int kArmPoseCount = 6;
const int kArmPoseDeg[kArmPoseCount][2] = {{0, 0},   {20, 45},  {45, -60},
                                           {-30, 90}, {70, 120}, {20, 45}};
const bool kArmPoseFlip[kArmPoseCount] = {false, false, false, false, false, true};

// pinned: the arm group at each pose, 1x and 2x2, on a 64x64 canvas cleared
// dark then on one cleared lit, fnv-1a over the dark bytes then on through
// the lit. regenerate only deliberately: a moved pin is a changelog entry
// and a version bump.
const uint64_t GOLDEN_GROUP[kArmPoseCount][2] = {
    {0xbcbbac4128ae9a62ULL, 0xbcbbac4128ae9a62ULL},  // 0, 0: 1x, 2x2
    {0xaf5185023697a357ULL, 0x28e9061d72beebe1ULL},  // 20, 45
    {0x44c57e3cf00411eeULL, 0xa1a569f669f690e0ULL},  // 45, -60
    {0x083d53b351b0a017ULL, 0xcda63b221929034eULL},  // -30, 90
    {0x901fa3582277adccULL, 0x1f6bc4e548e28b70ULL},  // 70, 120
    {0xc4b33be579d95530ULL, 0x74fe463e21b8320fULL},  // 20, 45 mirrored
};

// pinned: the arm group at each pose with its root at scale 0.9 and 1.1,
// 1x and 2x2, on the same canvases. regenerate only deliberately.
const int kArmScaleCount = 2;
const float kArmScale[kArmScaleCount] = {0.9f, 1.1f};
const uint64_t GOLDEN_GROUP_SCALED[kArmScaleCount][kArmPoseCount][2] = {
    {
        // 0.9
        {0xcd69da9198e7e012ULL, 0xcd69da9198e7e012ULL},  // 0, 0: 1x, 2x2
        {0x9ea6ee3625c4abe6ULL, 0xc4f15e3c66bd791cULL},  // 20, 45
        {0xd8a3d4628c05a024ULL, 0xde836144be487f33ULL},  // 45, -60
        {0x03169a088b632962ULL, 0x4c675874ba2c20b5ULL},  // -30, 90
        {0x3abe6e5bc13ea873ULL, 0x33a94f764ae575eaULL},  // 70, 120
        {0x654e74e18d9fce6aULL, 0x0d0fbd40d8e16340ULL},  // 20, 45 mirrored
    },
    {
        // 1.1
        {0x74e00769af459132ULL, 0x55ad42290b40daf7ULL},  // 0, 0: 1x, 2x2
        {0xe9b7502850c69884ULL, 0x1f974efcc749109bULL},  // 20, 45
        {0xe4123714f3afcc46ULL, 0xdca8a5b30c9854f6ULL},  // 45, -60
        {0xdaf9afff964971e8ULL, 0x5b600eb97be6b029ULL},  // -30, 90
        {0x50e922fa934ae2c2ULL, 0x20def039a40e4a3cULL},  // 70, 120
        {0xd2b4f6899c9acfbeULL, 0xeb3dfb0163f1fc8bULL},  // 20, 45 mirrored
    },
};

uint64_t armHash(int p, SD_Sample q, const char *label, float scale = 1.0f) {
  OwnedCanvas dark(kArmW, kArmH), lit(kArmW, kArmH), sil(kArmW, kArmH), ink(kArmW, kArmH);
  dark.canvas.clear(false);
  lit.canvas.clear(true);
  SD_Bone b[3];
  armBones(b, kArmPoseDeg[p][0], kArmPoseDeg[p][1], kArmPoseFlip[p]);
  if (scale != 1.0f) b[0].scale = scale;
  SD_Xform world[3];
  SD_Rig rig(b, world, 3);
  rig.pose();
  const int list[3] = {0, 1, 2};
  Record td(dark.canvas), tl(lit.canvas);
  const bool ok1 = sd_drawGroup(td, rig, list, 3, sil.canvas, ink.canvas, q);
  const bool ok2 = sd_drawGroup(tl, rig, list, 3, sil.canvas, ink.canvas, q);
  CHECK(ok1 && ok2 && td.oob == 0 && tl.oob == 0, "%s: drawn %d %d, out of bounds %ld %ld", label,
        ok1, ok2, td.oob, tl.oob);
  char name[96];
  std::snprintf(name, sizeof(name), "%s_dark", label);
  writeFrame(dark.canvas, name);
  std::snprintf(name, sizeof(name), "%s_lit", label);
  writeFrame(lit.canvas, name);
  return fnv1a(lit.canvas.buffer(), lit.canvas.bytes(),
               fnv1a(dark.canvas.buffer(), dark.canvas.bytes()));
}

// 8-connected pieces of the lit pixels of c inside [x0, x1) x [y0, y1)
int litPieces(const SD_Canvas &c, int x0, int y0, int x1, int y1) {
  const int w = x1 - x0, h = y1 - y0;
  std::vector<uint8_t> seen((size_t)w * (size_t)h, 0);
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      if (!c.pixel((int16_t)x, (int16_t)y) || seen[(size_t)(y - y0) * w + (x - x0)]) continue;
      ++n;
      std::vector<int> st(1, (y - y0) * w + (x - x0));
      seen[(size_t)st[0]] = 1;
      while (!st.empty()) {
        const int p = st.back();
        st.pop_back();
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const int xx = p % w + dx, yy = p / w + dy;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h || seen[(size_t)yy * w + xx]) continue;
            if (!c.pixel((int16_t)(xx + x0), (int16_t)(yy + y0))) continue;
            seen[(size_t)yy * w + xx] = 1;
            st.push_back(yy * w + xx);
          }
      }
    }
  return n;
}

}  // namespace

void runGroupTests() {
  // ---- silhouette and ink: sd_draw's own plotted and lit pixels ----
  {
    const int poses[7] = {0, 20, 45, 70, 90, 180, 20};
    long bad = 0;
    for (int f = 0; f < kFixtureCount; ++f)
      for (int p = 0; p < 7; ++p)
        for (int qi = 0; qi < 2; ++qi) {
          const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
          const SD_Xform m = SD_Xform::rigid(20.0f, 20.0f, degToRad((float)poses[p]), p == 6);
          OwnedCanvas drawn(40, 40), sil(40, 40), ink(40, 40);
          drawn.canvas.clear(false);
          sil.canvas.clear(false);
          ink.canvas.clear(false);
          Record r(drawn.canvas);
          const bool a = sd_draw(r, kFixtures[f]->sprite, m, q);
          const bool b = sd_drawSilhouette(sil.canvas, kFixtures[f]->sprite, m, q);
          const bool c = sd_drawInk(ink.canvas, kFixtures[f]->sprite, m, q);
          if (!a || !b || !c) ++bad;
          for (int16_t y = 0; y < 40; ++y)
            for (int16_t x = 0; x < 40; ++x)
              if (sil.canvas.pixel(x, y) != (r.at(x, y) != 0) ||
                  ink.canvas.pixel(x, y) != (r.at(x, y) == 2))
                ++bad;
        }
    CHECK(bad == 0, "silhouette and ink differ from sd_draw's plotted and lit pixels at %ld places",
          bad);
    OwnedCanvas sil(8, 8);
    SD_Sprite broken = kBlock.sprite;
    broken.w = 0;
    CHECK(!sd_drawSilhouette(sil.canvas, broken, SD_Xform::rigid(4, 4, 0.0f)) &&
              !sd_drawInk(sil.canvas, broken, SD_Xform::rigid(4, 4, 0.0f)),
          "a bad sprite's silhouette or ink not refused");
  }

  // ---- a lone rectangle: a 1 px outline, dark inside, the rest untouched ----
  {
    OwnedCanvas got(32, 24), bg(32, 24), sil(32, 24), ink(32, 24);
    checker(got.canvas);
    checker(bg.canvas);
    Record r(got.canvas);
    CHECK(groupOfOne(r, kBlock.sprite, 10.0f, 8.0f, 0.0f, sil.canvas, ink.canvas),
          "block: group refused");
    long bad = 0;
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 32; ++x) {
        const bool in = x >= 10 && x <= 17 && y >= 6 && y <= 10;
        const bool rim = in && (x == 10 || x == 17 || y == 6 || y == 10);
        const bool want = in ? rim : bg.canvas.pixel((int16_t)x, (int16_t)y);
        if (got.canvas.pixel((int16_t)x, (int16_t)y) != want || (r.at(x, y) != 0) != in) ++bad;
      }
    CHECK(bad == 0 && r.oob == 0,
          "block: %ld pixels not a 1 px outline round a dark inside, or touched outside", bad);
  }

  // ---- two overlapping rectangles: one contour, no line inside ----
  {
    OwnedCanvas got(32, 24), sil(32, 24), ink(32, 24);
    got.canvas.clear(false);
    const SD_Bone bones[2] = {{-1, 10.0f, 8.0f, 0.0f, false, false, &kBlock.sprite},
                              {-1, 14.0f, 10.0f, 0.0f, false, false, &kBlock.sprite}};
    SD_Xform world[2];
    SD_Rig rig(bones, world, 2);
    rig.pose();
    const int list[2] = {0, 1};
    CHECK(sd_drawGroup(got.canvas, rig, list, 2, sil.canvas, ink.canvas, SD_SAMPLE_1),
          "two blocks: group refused");
    auto inUnion = [](int x, int y) {
      return (x >= 10 && x <= 17 && y >= 6 && y <= 10) || (x >= 14 && x <= 21 && y >= 8 && y <= 12);
    };
    long bad = 0;
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 32; ++x) {
        const bool rim = inUnion(x, y) && (!inUnion(x - 1, y) || !inUnion(x + 1, y) ||
                                           !inUnion(x, y - 1) || !inUnion(x, y + 1));
        if (got.canvas.pixel((int16_t)x, (int16_t)y) != rim) ++bad;
      }
    CHECK(bad == 0, "two blocks: %ld pixels off the union's one contour", bad);
    CHECK(!got.canvas.pixel(17, 9) && !got.canvas.pixel(14, 9) && !got.canvas.pixel(16, 10),
          "two blocks: a block's own edge shows inside the other");
    CHECK(litPieces(got.canvas, 0, 0, 32, 24) == 1, "two blocks: the contour is %d pieces, not one",
          litPieces(got.canvas, 0, 0, 32, 24));
  }

  // ---- interior ink shows ----
  {
    OwnedCanvas got(32, 24), sil(32, 24), ink(32, 24);
    got.canvas.clear(false);
    CHECK(groupOfOne(got.canvas, kBlockInk.sprite, 10.0f, 8.0f, 0.0f, sil.canvas, ink.canvas),
          "ink: refused");
    long bad = 0;
    for (int y = 7; y <= 9; ++y)
      for (int x = 11; x <= 16; ++x) {
        const bool want = y == 8 && x >= 12 && x <= 15;
        if (got.canvas.pixel((int16_t)x, (int16_t)y) != want) ++bad;
      }
    CHECK(bad == 0 && got.canvas.pixel(10, 8) && got.canvas.pixel(17, 8),
          "ink: %ld interior pixels wrong", bad);
  }

  // ---- a shape at the canvas's edge is outlined there ----
  {
    OwnedCanvas got(32, 24), sil(32, 24), ink(32, 24);
    got.canvas.clear(false);
    CHECK(groupOfOne(got.canvas, kBlock.sprite, -3.0f, 1.0f, 0.0f, sil.canvas, ink.canvas),
          "edge: refused");
    bool left = true, top = true;
    for (int y = 0; y <= 3; ++y) left = left && got.canvas.pixel(0, (int16_t)y);
    for (int x = 0; x <= 4; ++x) top = top && got.canvas.pixel((int16_t)x, 0);
    CHECK(left && top && !got.canvas.pixel(1, 1) && !got.canvas.pixel(3, 2),
          "edge: a block off the top left corner not outlined along the canvas's edges, or lit "
          "inside");
  }

  // ---- a pinhole is the shape's: dark, no ring round it; a wider hole is outlined ----
  {
    OwnedCanvas got(16, 8), sil(16, 8), ink(16, 8);
    got.canvas.clear(true);
    Record r(got.canvas);
    CHECK(groupOfOne(r, kPinhole.sprite, 2.0f, 3.0f, 0.0f, sil.canvas, ink.canvas),
          "pinhole: refused");
    // the block spans x 2..6, y 2..4; the pinhole at (4, 3)
    CHECK(r.at(4, 3) == 1 && r.at(3, 3) == 1 && r.at(5, 3) == 1 && r.at(2, 3) == 2 &&
              r.at(6, 3) == 2,
          "pinhole: not filled dark inside one outline (%d %d %d, rim %d %d)", r.at(3, 3),
          r.at(4, 3), r.at(5, 3), r.at(2, 3), r.at(6, 3));
    OwnedCanvas got2(16, 8);
    got2.canvas.clear(true);
    Record r2(got2.canvas);
    CHECK(groupOfOne(r2, kHole2.sprite, 2.0f, 3.0f, 0.0f, sil.canvas, ink.canvas),
          "hole2: refused");
    // the block spans x 2..7; the hole at (4, 3) and (5, 3), its rim (3, 3) and (6, 3) outlined
    CHECK(r2.at(4, 3) == 0 && r2.at(5, 3) == 0 && r2.at(3, 3) == 2 && r2.at(6, 3) == 2,
          "hole2: a two-pixel hole filled, or its rim not outlined");
  }

  // ---- nothing outside the silhouette is touched; never out of bounds ----
  {
    long bad = 0, oob = 0;
    for (int bgk = 0; bgk < 3; ++bgk)
      for (int p = 0; p < kArmPoseCount; ++p)
        for (int qi = 0; qi < 2; ++qi) {
          const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
          OwnedCanvas got(kArmW, kArmH), bg(kArmW, kArmH), sil(kArmW, kArmH), ink(kArmW, kArmH),
              shape(kArmW, kArmH);
          if (bgk == 0) got.canvas.clear(false), bg.canvas.clear(false);
          if (bgk == 1) got.canvas.clear(true), bg.canvas.clear(true);
          if (bgk == 2) checker(got.canvas), checker(bg.canvas);
          SD_Bone b[3];
          armBones(b, kArmPoseDeg[p][0], kArmPoseDeg[p][1], kArmPoseFlip[p]);
          SD_Xform world[3];
          SD_Rig rig(b, world, 3);
          rig.pose();
          shape.canvas.clear(false);
          for (int i = 0; i < 3; ++i)
            sd_drawSilhouette(shape.canvas, *b[i].sprite, rig.world(i), q);
          const int list[3] = {0, 1, 2};
          Record r(got.canvas);
          sd_drawGroup(r, rig, list, 3, sil.canvas, ink.canvas, q);
          oob += r.oob;
          for (int16_t y = 0; y < kArmH; ++y)
            for (int16_t x = 0; x < kArmW; ++x)
              if (!inShape(shape.canvas, x, y) &&
                  (r.at(x, y) != 0 || got.canvas.pixel(x, y) != bg.canvas.pixel(x, y)))
                ++bad;
        }
    CHECK(bad == 0, "arm: %ld pixels outside the silhouette touched", bad);
    // half off every edge of a small target
    for (int e = 0; e < 4; ++e) {
      OwnedCanvas got(16, 12), sil(16, 12), ink(16, 12);
      got.canvas.clear(false);
      Record r(got.canvas);
      const float xs[4] = {-6.0f, 12.0f, 4.0f, 4.0f}, ys[4] = {6.0f, 6.0f, -2.0f, 11.0f};
      groupOfOne(r, kSeg.sprite, xs[e], ys[e], 30.0f, sil.canvas, ink.canvas, SD_SAMPLE_2x2);
      oob += r.oob;
    }
    CHECK(oob == 0, "group: %ld plots outside the target", oob);
  }

  // ---- refusals ----
  {
    OwnedCanvas got(16, 12), sil(16, 12), ink(16, 12);
    got.canvas.clear(false);
    const SD_Bone bones[2] = {{-1, 6.0f, 6.0f, 0.0f, false, false, &kBlock.sprite},
                              {0, 2.0f, 0.0f, 0.0f, false, false, &kBlock.sprite}};
    SD_Xform world[2];
    SD_Rig rig(bones, world, 2);
    const int one[1] = {0}, past[2] = {0, 2}, before[1] = {-1};
    Record r(got.canvas);
    CHECK(!sd_drawGroup(r, rig, one, 1, sil.canvas, ink.canvas),
          "an unposed rig not refused");
    rig.pose();
    SD_Xform nw[2];
    SD_Rig noBones(nullptr, nw, 2);
    noBones.pose();
    CHECK(!sd_drawGroup(r, noBones, one, 1, sil.canvas, ink.canvas) &&
              !sd_drawGroup(r, rig, nullptr, 1, sil.canvas, ink.canvas) &&
              !sd_drawGroup(r, rig, one, -1, sil.canvas, ink.canvas) &&
              !sd_drawGroup(r, rig, past, 2, sil.canvas, ink.canvas) &&
              !sd_drawGroup(r, rig, before, 1, sil.canvas, ink.canvas),
          "a rig over no bones, a null list, a negative count or an index outside the rig not "
          "refused");
    long touched = 0;
    for (int16_t y = 0; y < 12; ++y)
      for (int16_t x = 0; x < 16; ++x) touched += r.at(x, y) != 0;
    CHECK(touched == 0, "a refused group drew %ld pixels", touched);
    CHECK(sd_drawGroup(r, rig, past, 0, sil.canvas, ink.canvas), "an empty group refused");
    SD_Sprite broken = kBlock.sprite;
    broken.h = 0;
    const SD_Bone mixed[2] = {{-1, 6.0f, 6.0f, 0.0f, false, false, &kBlock.sprite},
                              {-1, 3.0f, 3.0f, 0.0f, false, false, &broken}};
    SD_Xform mw[2];
    SD_Rig mrig(mixed, mw, 2);
    mrig.pose();
    const int both[2] = {0, 1};
    CHECK(!sd_drawGroup(r, mrig, both, 2, sil.canvas, ink.canvas) && r.at(8, 6) == 1 &&
              r.at(6, 4) == 2,
          "a group with a bad sprite: not false, or its good piece not drawn");
  }

  // ---- golden: the arm group at the pinned poses ----
  for (int p = 0; p < kArmPoseCount; ++p)
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      char label[96];
      std::snprintf(label, sizeof(label), "group_%d_%d%s_%s", kArmPoseDeg[p][0], kArmPoseDeg[p][1],
                    kArmPoseFlip[p] ? "_mirrored" : "", qi == 0 ? "1x" : "2x2");
      const uint64_t h = armHash(p, q, label);
      CHECK(h == GOLDEN_GROUP[p][qi], "%s: hash 0x%016llxULL != pinned 0x%016llxULL", label,
            (unsigned long long)h, (unsigned long long)GOLDEN_GROUP[p][qi]);
    }

  // ---- scale: the arm through a root of scale 0.9 and 1.1 is one shape with
  // one 1 px outline at every pose: drawn on dark, a pixel is lit exactly
  // where it is the shape's edge (its silhouettes, drawn apart, and their
  // pinholes) or ink inside it, the shape is one 4-connected piece and its
  // edge one 8-connected piece ----
  {
    long wrong = 0, outside = 0;
    int shapes = 0, contours = 0, frames = 0;
    for (int k = 0; k < kArmScaleCount; ++k)
      for (int p = 0; p < kArmPoseCount; ++p)
        for (int qi = 0; qi < 2; ++qi) {
          const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
          OwnedCanvas got(kArmW, kArmH), sil(kArmW, kArmH), ink(kArmW, kArmH), shape(kArmW, kArmH),
              inks(kArmW, kArmH), edge(kArmW, kArmH);
          got.canvas.clear(false);
          shape.canvas.clear(false);
          inks.canvas.clear(false);
          edge.canvas.clear(false);
          SD_Bone b[3];
          armBones(b, kArmPoseDeg[p][0], kArmPoseDeg[p][1], kArmPoseFlip[p]);
          b[0].scale = kArmScale[k];
          SD_Xform world[3];
          SD_Rig rig(b, world, 3);
          rig.pose();
          for (int i = 0; i < 3; ++i) {
            sd_drawSilhouette(shape.canvas, *b[i].sprite, rig.world(i), q);
            sd_drawInk(inks.canvas, *b[i].sprite, rig.world(i), q);
          }
          const int list[3] = {0, 1, 2};
          Record r(got.canvas);
          sd_drawGroup(r, rig, list, 3, sil.canvas, ink.canvas, q);
          for (int y = 0; y < kArmH; ++y)
            for (int x = 0; x < kArmW; ++x) {
              const bool in = inShape(shape.canvas, x, y);
              const SD_Canvas &c = shape.canvas;
              const bool rim = in && (!inShape(c, x - 1, y) || !inShape(c, x + 1, y) ||
                                      !inShape(c, x, y - 1) || !inShape(c, x, y + 1));
              if (rim) edge.canvas.plot((int16_t)x, (int16_t)y, true);
              const bool want = rim || (in && inks.canvas.pixel((int16_t)x, (int16_t)y));
              if (got.canvas.pixel((int16_t)x, (int16_t)y) != want) ++wrong;
              if (!in && r.at(x, y) != 0) ++outside;
            }
          // the shape's 4-connected pieces, its set pixels and pinholes
          OwnedCanvas full(kArmW, kArmH);
          full.canvas.clear(false);
          for (int y = 0; y < kArmH; ++y)
            for (int x = 0; x < kArmW; ++x)
              if (inShape(shape.canvas, x, y)) full.canvas.plot((int16_t)x, (int16_t)y, true);
          int pieces4 = 0;
          {
            std::vector<uint8_t> seen((size_t)kArmW * kArmH, 0);
            for (int i = 0; i < kArmW * kArmH; ++i) {
              const int16_t ix = (int16_t)(i % kArmW), iy = (int16_t)(i / kArmW);
              if (seen[(size_t)i] || !full.canvas.pixel(ix, iy)) continue;
              ++pieces4;
              std::vector<int> st(1, i);
              seen[(size_t)i] = 1;
              while (!st.empty()) {
                const int at = st.back();
                st.pop_back();
                const int nx[4] = {at % kArmW - 1, at % kArmW + 1, at % kArmW, at % kArmW};
                const int ny[4] = {at / kArmW, at / kArmW, at / kArmW - 1, at / kArmW + 1};
                for (int d = 0; d < 4; ++d) {
                  if (nx[d] < 0 || ny[d] < 0 || nx[d] >= kArmW || ny[d] >= kArmH) continue;
                  const int qq = ny[d] * kArmW + nx[d];
                  if (seen[(size_t)qq] || !full.canvas.pixel((int16_t)nx[d], (int16_t)ny[d]))
                    continue;
                  seen[(size_t)qq] = 1;
                  st.push_back(qq);
                }
              }
            }
          }
          shapes += pieces4 == 1 ? 1 : 0;
          contours += litPieces(edge.canvas, 0, 0, kArmW, kArmH) == 1 ? 1 : 0;
          ++frames;
        }
    CHECK(wrong == 0 && outside == 0,
          "scaled arm: %ld pixels off the shape's one 1 px outline and its ink, %ld touched "
          "outside it",
          wrong, outside);
    CHECK(shapes == frames && contours == frames,
          "scaled arm: one shape in %d of %d frames, one outline in %d", shapes, frames, contours);
  }

  // ---- golden: the arm group through a scaled root ----
  for (int k = 0; k < kArmScaleCount; ++k)
    for (int p = 0; p < kArmPoseCount; ++p)
      for (int qi = 0; qi < 2; ++qi) {
        const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        char label[96];
        std::snprintf(label, sizeof(label), "group_scaled_%g_%d_%d%s_%s", kArmScale[k],
                      kArmPoseDeg[p][0], kArmPoseDeg[p][1], kArmPoseFlip[p] ? "_mirrored" : "",
                      qi == 0 ? "1x" : "2x2");
        const uint64_t h = armHash(p, q, label, kArmScale[k]);
        CHECK(h == GOLDEN_GROUP_SCALED[k][p][qi], "%s: hash 0x%016llxULL != pinned 0x%016llxULL",
              label, (unsigned long long)h, (unsigned long long)GOLDEN_GROUP_SCALED[k][p][qi]);
      }

  // ---- a piece of scale 0 is left out of its group: the rest drawn, false returned ----
  {
    OwnedCanvas got(kArmW, kArmH), want(kArmW, kArmH), sil(kArmW, kArmH), ink(kArmW, kArmH);
    got.canvas.clear(false);
    want.canvas.clear(false);
    SD_Bone b[3];
    armBones(b, 20, 45, false);
    SD_Xform world[3];
    SD_Rig rig(b, world, 3);
    rig.pose();
    const int two[2] = {0, 1}, all[3] = {0, 1, 2};
    CHECK(sd_drawGroup(want.canvas, rig, two, 2, sil.canvas, ink.canvas),
          "the arm's first two pieces: group refused");
    b[2].scale = 0.0f;
    rig.pose();
    CHECK(!sd_drawGroup(got.canvas, rig, all, 3, sil.canvas, ink.canvas) &&
              memcmp(got.canvas.buffer(), want.canvas.buffer(), got.canvas.bytes()) == 0,
          "a group with a piece of scale 0: not false, or not its other pieces alone");
  }

  // ---- a piece of scale 0 posed onto (0, 0), its world matrix all zero, is posed:
  // left out, the rest drawn; a bone pose() refused, and those after it, are not ----
  {
    OwnedCanvas got(kArmW, kArmH), want(kArmW, kArmH), sil(kArmW, kArmH), ink(kArmW, kArmH);
    got.canvas.clear(false);
    want.canvas.clear(false);
    SD_Bone b[3] = {{-1, 0.0f, 0.0f, 0.0f, false, false, &kSeg.sprite, 0.0f},
                    {-1, 30.0f, 30.0f, degToRad(20.0f), false, false, &kSeg.sprite},
                    {-1, 40.0f, 20.0f, 0.0f, false, false, &kBlock.sprite}};
    SD_Xform world[3];
    SD_Rig rig(b, world, 3);
    rig.pose();
    const int one[1] = {1}, both[2] = {0, 1}, last[1] = {2};
    CHECK(sd_drawGroup(want.canvas, rig, one, 1, sil.canvas, ink.canvas),
          "a lone segment: group refused");
    CHECK(!sd_drawGroup(got.canvas, rig, both, 2, sil.canvas, ink.canvas) &&
              memcmp(got.canvas.buffer(), want.canvas.buffer(), got.canvas.bytes()) == 0,
          "a group with a piece of scale 0 at (0, 0): not false, or not its other piece alone");
    b[1].parent = 1;
    got.canvas.clear(false);
    Record r(got.canvas);
    CHECK(!rig.pose() && !sd_drawGroup(r, rig, one, 1, sil.canvas, ink.canvas) &&
              !sd_drawGroup(r, rig, last, 1, sil.canvas, ink.canvas),
          "a bone pose() refused, or one after it, not refused by the group");
    long touched = 0;
    for (int16_t y = 0; y < kArmH; ++y)
      for (int16_t x = 0; x < kArmW; ++x) touched += r.at(x, y) != 0;
    CHECK(touched == 0, "a group of bones pose() refused drew %ld pixels", touched);
  }
}
