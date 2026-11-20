// SPDX-License-Identifier: MIT
#ifndef SD_TEST_FIXTURES_H
#define SD_TEST_FIXTURES_H

// the fixture sprites, authored as ascii rows, and the helpers the suites
// share: exact-size heap copies (so asan sees any read past a plane), a
// reference drawBitmap-style copy, and the mockup frame's layout.

#include <stdint.h>
#include <string.h>

#include <initializer_list>
#include <memory>

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Sprite.h"
#include "SD_Xform.h"

// a sprite authored as ascii rows: '#' ink (masked, lit), '-' black (masked,
// dark), '.' unmasked. the constructor packs the rows into drawBitmap-layout
// planes at static init.
struct ArtFixture {
  static const int kMaxRows = 16;
  static const int kMaxBytes = 64;

  const char *name;
  const char *rows[kMaxRows];
  bool ok;  // rows present, all one width, within capacity, legal characters
  uint8_t ink[kMaxBytes];
  uint8_t mask[kMaxBytes];
  SD_Sprite sprite;

  ArtFixture(const char *nm, std::initializer_list<const char *> art, int16_t px, int16_t py)
      : name(nm), rows(), ok(true), ink(), mask(), sprite() {
    const int h = (int)art.size();
    const int w = h > 0 ? (int)strlen(*art.begin()) : 0;
    const int stride = (w + 7) / 8;
    if (h < 1 || h > kMaxRows || w < 1 || stride * h > kMaxBytes) ok = false;
    int y = 0;
    for (const char *row : art) {
      if (!ok) break;
      if ((int)strlen(row) != w) ok = false;
      rows[y] = row;
      for (int x = 0; ok && x < w; ++x) {
        const uint8_t bit = (uint8_t)(0x80u >> (x & 7));
        const int at = y * stride + x / 8;
        if (row[x] == '#') {
          mask[at] |= bit;
          ink[at] |= bit;
        } else if (row[x] == '-') {
          mask[at] |= bit;
        } else if (row[x] != '.') {
          ok = false;
        }
      }
      ++y;
    }
    sprite = {(int16_t)w, (int16_t)h, px, py, ink, mask};
  }
  ArtFixture(const ArtFixture &) = delete;
  ArtFixture &operator=(const ArtFixture &) = delete;

  // the authored character at (x, y); '.' outside the art.
  char at(int x, int y) const {
    if (x < 0 || y < 0 || x >= sprite.w || y >= sprite.h) return '.';
    return rows[y][x];
  }
};

// an outlined arm, pivot on its shoulder pixel (0, 3).
inline const ArtFixture kArm("arm",
                             {
                                 "..############..",
                                 ".#............#.",
                                 "#..............#",
                                 "#..............#",
                                 "#..............#",
                                 ".#............#.",
                                 "..############..",
                             },
                             0, 3);

// a mallet, pivot on the end of its handle (0, 2).
inline const ArtFixture kMallet("mallet",
                                {
                                    ".......#####",
                                    ".......#####",
                                    "########....",
                                    ".......#####",
                                    ".......#####",
                                },
                                0, 2);

// a flag on a pole: white outline, black cloth, unmasked sky; lopsided in
// both axes so a wrong mirror or turn shows. pivot at the foot of the pole.
inline const ArtFixture kFlag("flag",
                              {
                                  "##.......",
                                  "#-####...",
                                  "#-----##.",
                                  "#-------#",
                                  "#-----##.",
                                  "#-####...",
                                  "##.......",
                                  "#........",
                                  "#........",
                                  "#........",
                                  "#........",
                              },
                              0, 10);

inline const ArtFixture *const kFixtures[] = {&kArm, &kMallet, &kFlag};
inline constexpr int kFixtureCount = 3;

// a sprite whose planes are exactly-sized heap blocks, so asan reports any
// read past either plane.
struct HeapSprite {
  std::unique_ptr<uint8_t[]> ink, mask;
  SD_Sprite sprite;
  explicit HeapSprite(const SD_Sprite &s) : sprite(s) {
    const size_t n = (size_t)((s.w + 7) / 8) * (size_t)s.h;
    ink.reset(new uint8_t[n]);
    mask.reset(new uint8_t[n]);
    memcpy(ink.get(), s.ink, n);
    memcpy(mask.get(), s.mask, n);
    sprite.ink = ink.get();
    sprite.mask = mask.get();
  }
};

// a canvas with its own buffer.
struct OwnedCanvas {
  std::unique_ptr<uint8_t[]> buf;
  SD_Canvas canvas;
  OwnedCanvas(int16_t w, int16_t h)
      : buf(new uint8_t[(size_t)((w + 7) / 8) * (size_t)h]()), canvas(buf.get(), w, h) {}
};

// adafruit_gfx drawBitmap's loop over one plane, in its row layout: every set
// bit plots `on` at (x + i, y + j).
inline void referenceBitmap(SD_Canvas &c, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w,
                            int16_t h, bool on) {
  const int16_t byteWidth = (int16_t)((w + 7) / 8);
  uint8_t b = 0;
  for (int16_t j = 0; j < h; ++j) {
    for (int16_t i = 0; i < w; ++i) {
      if (i & 7)
        b = (uint8_t)(b << 1);
      else
        b = bitmap[j * byteWidth + i / 8];
      if (b & 0x80) c.plot((int16_t)(x + i), (int16_t)(y + j), on);
    }
  }
}

// how a gfx sketch draws a two-plane sprite unrotated with its pivot at
// (x, y): the mask dark, then the ink lit.
inline void referenceDraw(SD_Canvas &c, const SD_Sprite &s, int16_t x, int16_t y) {
  referenceBitmap(c, (int16_t)(x - s.pivotX), (int16_t)(y - s.pivotY), s.mask, s.w, s.h, false);
  referenceBitmap(c, (int16_t)(x - s.pivotX), (int16_t)(y - s.pivotY), s.ink, s.w, s.h, true);
}

inline float degToRad(float deg) { return deg * 0.017453292519943295f; }

// the mockup frame, 160x80: the arm and the mallet in tiles 40 px wide at 0,
// 20, 45 and 70 degrees, the top row SD_SAMPLE_1 and the bottom row
// SD_SAMPLE_2x2. the arm's pivot, its shoulder pixel (0, 3), sits at
// (tile * 40 + 8, row * 40 + 20) and the mallet's pivot (0, 2) at
// (round(15 cos a), round(15 sin a)) from it, at the same angle.
inline constexpr int kMockupW = 160;
inline constexpr int kMockupH = 80;
inline constexpr int kMockupDeg[4] = {0, 20, 45, 70};
inline constexpr int kMockupMallet[4][2] = {{15, 0}, {14, 5}, {11, 11}, {5, 14}};

// pinned: the mockup frame drawn by drawMockup on a canvas cleared dark. the
// draw suite pins it with the ascii arm, the converter suite with the arm
// converted from arm.png.
inline constexpr uint64_t GOLDEN_MOCKUP = 0x273a8853f28c84adULL;

inline void drawMockup(SD_Canvas &c, const SD_Sprite &armSprite, const SD_Sprite *mallet) {
  for (int row = 0; row < 2; ++row) {
    const SD_Sample q = row == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
    for (int i = 0; i < 4; ++i) {
      const float ang = degToRad((float)kMockupDeg[i]);
      const int ox = i * 40 + 8;
      const int oy = row * 40 + 20;
      sd_draw(c, armSprite, SD_Xform::rigid((float)ox, (float)oy, ang), q);
      if (mallet != nullptr)
        sd_draw(c, *mallet,
                SD_Xform::rigid((float)(ox + kMockupMallet[i][0]),
                                (float)(oy + kMockupMallet[i][1]), ang),
                q);
    }
  }
}

#endif  // SD_TEST_FIXTURES_H
