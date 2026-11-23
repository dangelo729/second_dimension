// SPDX-License-Identifier: MIT

#include "SD_Group.h"

#include "SD_Internal.h"

namespace {

// sd_draw's plots into a scratch canvas: every plot sets the pixel (the
// shape), or only a lit plot does (the ink).
class Scratch : public SD_Target {
 public:
  Scratch(SD_Canvas &c, bool inkOnly) : c_(c), inkOnly_(inkOnly) {}
  int16_t width() const override { return c_.width(); }
  int16_t height() const override { return c_.height(); }
  void plot(int16_t x, int16_t y, bool on) override {
    if (on || !inkOnly_) c_.plot(x, y, true);
  }

 private:
  SD_Canvas &c_;
  bool inkOnly_;
};

// sd_draw's box for a sprite through m, unclipped; false for a box that is
// not finite.
bool boxOf(const SD_Sprite &s, const SD_Xform &m, float &bx0, float &by0, float &bx1, float &by1) {
  sdBox(s, m, bx0, by0, bx1, by1);
  return bx0 == bx0 && by0 == by0 && bx1 == bx1 && by1 == by1 && bx0 > -1e9f && by0 > -1e9f &&
         bx1 < 1e9f && by1 < 1e9f;
}

int mini(int a, int b) { return a < b ? a : b; }
int maxi(int a, int b) { return a > b ? a : b; }

}  // namespace

bool sd_drawSilhouette(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                       SD_Sample sample) {
  Scratch t(scratch, false);
  return sd_draw(t, sprite, xform, sample);
}

bool sd_drawInk(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                SD_Sample sample) {
  Scratch t(scratch, true);
  return sd_draw(t, sprite, xform, sample);
}

namespace {

// in the shape: a silhouette pixel, or a pinhole, an unset pixel whose four
// neighbours are all set.
bool inShape(const SD_Canvas &s, int x, int y) {
  const int16_t px = (int16_t)x, py = (int16_t)y;
  if (s.pixel(px, py)) return true;
  return s.pixel((int16_t)(x - 1), py) && s.pixel((int16_t)(x + 1), py) &&
         s.pixel(px, (int16_t)(y - 1)) && s.pixel(px, (int16_t)(y + 1));
}

}  // namespace

void sd_stampOutline(SD_Target &target, const SD_Canvas &silhouette, const SD_Canvas &ink, int x0,
                     int y0, int x1, int y1) {
  x0 = maxi(x0, 0);
  y0 = maxi(y0, 0);
  x1 = mini(x1, mini(target.width(), silhouette.width()));
  y1 = mini(y1, mini(target.height(), silhouette.height()));
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (!inShape(silhouette, x, y)) continue;
      const bool edge = !inShape(silhouette, x - 1, y) || !inShape(silhouette, x + 1, y) ||
                        !inShape(silhouette, x, y - 1) || !inShape(silhouette, x, y + 1);
      target.plot((int16_t)x, (int16_t)y, edge || ink.pixel((int16_t)x, (int16_t)y));
    }
  }
}

bool sd_drawGroup(SD_Target &target, const SD_Rig &rig, const int *list, int n,
                  SD_Canvas &silhouette, SD_Canvas &ink, SD_Sample sample) {
  if (list == nullptr || n < 0) return false;
  for (int k = 0; k < n; ++k)
    if (list[k] < 0 || list[k] >= rig.posed_) return false;

  // the box: every listed bone's, clipped to the target and both canvases,
  // cleared grown by two (the stamp reads a pixel's neighbours' neighbours)
  // and stamped grown by one.
  const int w = mini(target.width(), mini(silhouette.width(), ink.width()));
  const int h = mini(target.height(), mini(silhouette.height(), ink.height()));
  bool any = false;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  for (int k = 0; k < n; ++k) {
    const SD_Sprite *s = rig.bones_[list[k]].sprite;
    float bx0, by0, bx1, by1;
    if (s == nullptr || !sd_validateSprite(*s) ||
        !boxOf(*s, rig.world(list[k]), bx0, by0, bx1, by1))
      continue;
    x0 = any ? mini(x0, (int)bx0) : (int)bx0;
    y0 = any ? mini(y0, (int)by0) : (int)by0;
    x1 = any ? maxi(x1, (int)bx1) : (int)bx1;
    y1 = any ? maxi(y1, (int)by1) : (int)by1;
    any = true;
  }

  bool ok = true;
  if (any) {
    const int cx0 = maxi(x0 - 2, 0), cy0 = maxi(y0 - 2, 0);
    const int cx1 = mini(x1 + 2, w - 1), cy1 = mini(y1 + 2, h - 1);
    for (int y = cy0; y <= cy1; ++y)
      for (int x = cx0; x <= cx1; ++x) {
        silhouette.plot((int16_t)x, (int16_t)y, false);
        ink.plot((int16_t)x, (int16_t)y, false);
      }
  }
  for (int k = 0; k < n; ++k) {
    const SD_Sprite *s = rig.bones_[list[k]].sprite;
    if (s == nullptr) continue;
    const SD_Xform &m = rig.world(list[k]);
    ok = sd_drawSilhouette(silhouette, *s, m, sample) && ok;
    ok = sd_drawInk(ink, *s, m, sample) && ok;
  }
  if (any) sd_stampOutline(target, silhouette, ink, x0 - 1, y0 - 1, x1 + 2, y1 + 2);
  return ok;
}
