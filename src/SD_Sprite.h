// SPDX-License-Identifier: MIT
#ifndef SD_SPRITE_H
#define SD_SPRITE_H

#include <stdint.h>

#include "SD_Config.h"

// the sprite header format this library reads. tools/png2sprite.py writes
// it in every header's banner and checks it at compile time, so a stale
// header is a readable compile error. the layout below never changes: a new
// layout is a new struct and a new format number.
constexpr int kSDSpriteFormat = 1;

// sprite format 1, frozen. a non-owning view over two 1-bit planes: const
// arrays (the converter's headers) or ram (anything built at run time).
// generated headers initialize it positionally, so the fields are never
// reordered or retyped.
//
// both planes use adafruit_gfx drawBitmap's row layout: (w + 7) / 8 bytes
// per row, rows top to bottom, the leftmost pixel in the most significant
// bit. pixel (x, y) is bit 0x80 >> (x & 7) of byte y * ((w + 7) / 8) + x / 8.
//
// a pixel with its mask bit clear is not painted, whatever its ink bit says;
// a masked pixel paints white where its ink bit is set and black where it is
// clear. integer coordinates name pixel centres, and the pivot is the pixel
// whose centre a transform's origin sits on.
struct SD_Sprite {
  int16_t w, h;             // pixels
  int16_t pivotX, pivotY;   // the pixel the transform's origin sits on
  const uint8_t *ink;       // 1 bpp, rows padded to bytes, msb first
  const uint8_t *mask;      // same layout; 1 = this pixel is painted
};

// the structural check: both planes non-null, w and h at least 1, the pivot
// inside [0, w) x [0, h). it costs a handful of compares, and sd_draw runs
// it on every call.
bool sd_validateSprite(const SD_Sprite &s);

// reads pixel (x, y): returns whether it is masked and sets ink to its ink
// bit (false when unmasked). a coordinate outside [0, w) x [0, h) reads as
// unmasked without touching either plane.
static inline bool sd_spritePixel(const SD_Sprite &s, int16_t x, int16_t y, bool &ink) {
  ink = false;
  if (x < 0 || y < 0 || x >= s.w || y >= s.h) return false;
  const int32_t at = (int32_t)y * (((int32_t)s.w + 7) >> 3) + (x >> 3);
  const uint8_t bit = (uint8_t)(0x80u >> (x & 7));
  if ((s.mask[at] & bit) == 0) return false;
  ink = (s.ink[at] & bit) != 0;
  return true;
}

#endif  // SD_SPRITE_H
