// SPDX-License-Identifier: MIT
// sprite suite: the format (sd_validateSprite accepts the fixtures and
// refuses a null plane, a zero size, a pivot outside the sprite; the pixel
// reader reads the authored art and nothing past it) and the mask (a black
// pixel paints black over white, an unmasked pixel leaves the canvas alone,
// an ink bit under a clear mask bit is never painted).

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Sprite.h"

#include "test_common.h"
#include "test_fixtures.h"

// fills a canvas with a one-pixel checker: (x + y) odd is lit.
static void checker(SD_Canvas &c) {
  for (int16_t y = 0; y < c.height(); ++y)
    for (int16_t x = 0; x < c.width(); ++x) c.plot(x, y, ((x + y) & 1) != 0);
}

void runSpriteTests() {
  CHECK(kSDSpriteFormat == 1, "sprite format is %d, frozen at 1", kSDSpriteFormat);

  // ---- fixtures are well formed and valid ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    CHECK(fx.ok, "%s: ascii art malformed", fx.name);
    CHECK(sd_validateSprite(fx.sprite), "%s: fixture fails sd_validateSprite", fx.name);
  }

  // ---- validation refuses each broken view ----
  {
    const SD_Sprite good = kFlag.sprite;
    SD_Sprite s = good;
    CHECK(sd_validateSprite(s), "base sprite should validate");
    s = good; s.ink = nullptr;
    CHECK(!sd_validateSprite(s), "null ink plane accepted");
    s = good; s.mask = nullptr;
    CHECK(!sd_validateSprite(s), "null mask plane accepted");
    s = good; s.w = 0;
    CHECK(!sd_validateSprite(s), "zero width accepted");
    s = good; s.h = 0;
    CHECK(!sd_validateSprite(s), "zero height accepted");
    s = good; s.w = -3;
    CHECK(!sd_validateSprite(s), "negative width accepted");
    s = good; s.pivotX = -1;
    CHECK(!sd_validateSprite(s), "pivot left of the sprite accepted");
    s = good; s.pivotX = s.w;
    CHECK(!sd_validateSprite(s), "pivot right of the sprite accepted");
    s = good; s.pivotY = -1;
    CHECK(!sd_validateSprite(s), "pivot above the sprite accepted");
    s = good; s.pivotY = s.h;
    CHECK(!sd_validateSprite(s), "pivot below the sprite accepted");
    s = good; s.pivotX = (int16_t)(s.w - 1); s.pivotY = (int16_t)(s.h - 1);
    CHECK(sd_validateSprite(s), "pivot on the last pixel refused");
    s = good; s.pivotX = 0; s.pivotY = 0;
    CHECK(sd_validateSprite(s), "pivot on the first pixel refused");
  }

  // ---- the pixel reader reads the art, and nothing outside it ----
  for (int f = 0; f < kFixtureCount; ++f) {
    const ArtFixture &fx = *kFixtures[f];
    HeapSprite hs(fx.sprite);  // exact-size planes: asan sees any stray read
    const SD_Sprite &s = hs.sprite;
    long bad = 0;
    for (int y = -2; y < s.h + 2; ++y)
      for (int x = -2; x < s.w + 2; ++x) {
        bool ink = true;
        const bool masked = sd_spritePixel(s, (int16_t)x, (int16_t)y, ink);
        const char want = fx.at(x, y);
        if (masked != (want != '.') || ink != (want == '#')) ++bad;
      }
    CHECK(bad == 0, "%s: sd_spritePixel disagrees with the art at %ld pixels", fx.name, bad);
    bool ink = true;
    CHECK(!sd_spritePixel(s, -32768, -32768, ink) && !ink, "%s: far corner reads masked", fx.name);
    CHECK(!sd_spritePixel(s, 32767, 32767, ink) && !ink, "%s: far corner reads masked", fx.name);
  }

  // ---- mask: black over white, unmasked leaves the canvas alone ----
  {
    const SD_Sprite &s = kFlag.sprite;
    const int16_t W = 24, H = 24, X = 8, Y = 16;  // pivot (0, 10) at (8, 16)
    for (int bg = 0; bg < 3; ++bg) {
      for (int q = 0; q < 2; ++q) {
        const SD_Sample sample = q == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
        OwnedCanvas oc(W, H);
        SD_Canvas &c = oc.canvas;
        if (bg == 0) c.clear(false);
        if (bg == 1) c.clear(true);
        if (bg == 2) checker(c);
        const bool ok = sd_draw(c, s, SD_Xform::rigid(X, Y, 0.0f), sample);
        CHECK(ok, "mask: sd_draw returned false");
        long wrongInk = 0, wrongBlack = 0, wrongAlone = 0;
        for (int16_t y = 0; y < H; ++y)
          for (int16_t x = 0; x < W; ++x) {
            const char a = kFlag.at(x - X + s.pivotX, y - Y + s.pivotY);
            const bool under = bg == 0 ? false : bg == 1 ? true : ((x + y) & 1) != 0;
            const bool got = c.pixel(x, y);
            if (a == '#' && !got) ++wrongInk;
            if (a == '-' && got) ++wrongBlack;
            if (a == '.' && got != under) ++wrongAlone;
          }
        static const char *bgName[3] = {"dark", "lit", "checker"};
        CHECK(wrongInk == 0, "mask (%s, %s): %ld ink pixels not lit", bgName[bg],
              q == 0 ? "1x" : "2x2", wrongInk);
        CHECK(wrongBlack == 0, "mask (%s, %s): %ld black pixels not dark", bgName[bg],
              q == 0 ? "1x" : "2x2", wrongBlack);
        CHECK(wrongAlone == 0, "mask (%s, %s): %ld unmasked pixels changed the canvas",
              bgName[bg], q == 0 ? "1x" : "2x2", wrongAlone);
      }
    }
  }

  // ---- an ink bit under a clear mask bit is never painted ----
  {
    static const uint8_t ink[2] = {0xff, 0xff};
    static const uint8_t mask[2] = {0x00, 0x00};
    const SD_Sprite ghost = {8, 2, 0, 0, ink, mask};
    for (int q = 0; q < 2; ++q) {
      OwnedCanvas oc(16, 8);
      oc.canvas.clear(false);
      sd_draw(oc.canvas, ghost, SD_Xform::rigid(4.0f, 3.0f, 0.3f),
              q == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2);
      long lit = 0;
      for (size_t i = 0; i < oc.canvas.bytes(); ++i) lit += oc.canvas.buffer()[i] != 0;
      CHECK(lit == 0, "unmasked ink painted (%ld bytes changed)", lit);
    }
  }
}
