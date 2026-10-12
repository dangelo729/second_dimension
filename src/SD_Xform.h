// SPDX-License-Identifier: MIT
#ifndef SD_XFORM_H
#define SD_XFORM_H

#include "SD_Config.h"

// a 2x3 affine, target pixels from sprite pixels:
//     x' = a x + b y + tx
//     y' = c x + d y + ty
// in adafruit_gfx's frame: x right, y down, so a positive angle turns
// clockwise on the panel. integer coordinates name pixel centres.
//
// a sprite's transform maps pivot-relative coordinates: sd_draw subtracts
// the sprite's pivot from a sprite pixel before the matrix, so the origin of
// the matrix is the pivot pixel's centre and the constructors never need to
// know the pivot.
//
// the exact float expressions here and in sd_draw are pinned by the host
// suite's golden hashes: never reorder or regroup them.
struct SD_Xform {
  float a, b, c, d, tx, ty;

  // a turn by angle (radians, clockwise on the panel) about the origin,
  // mirrored in x first when flipX, then a move to (x, y): the pivot lands
  // on (x, y). a sprite pixel (sx, sy) of a sprite with pivot (px, py) goes
  // to dx = sx - px (negated when flipX), dy = sy - py, then
  //     x' = x + cos(angle) dx - sin(angle) dy
  //     y' = y + sin(angle) dx + cos(angle) dy
  static SD_Xform rigid(float x, float y, float angle, bool flipX = false);

  // child first, then parent: compose(p, q).apply(v) is p.apply(q.apply(v)).
  static SD_Xform compose(const SD_Xform &parent, const SD_Xform &child);

  // the inverse matrix. a matrix with no finite inverse (determinant zero,
  // or any entry of the result not finite) inverts to all zeros, which no
  // invertible matrix's inverse can be.
  SD_Xform inverse() const;

  void apply(float x, float y, float *ox, float *oy) const {
    *ox = a * x + b * y + tx;
    *oy = c * x + d * y + ty;
  }
};

#endif  // SD_XFORM_H
