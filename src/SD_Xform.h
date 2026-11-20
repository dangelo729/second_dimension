// SPDX-License-Identifier: MIT
#ifndef SD_XFORM_H
#define SD_XFORM_H

#include "SD_Config.h"

// a 2x3 affine, target pixels from sprite pixels:
//     x' = a x + b y + tx
//     y' = c x + d y + ty
// in adafruit_gfx's frame: x right, y down, so a positive angle turns
// clockwise on the panel. integer coordinates name pixel centres. sd_draw
// subtracts the sprite's pivot before the matrix, so the matrix's origin is
// the pivot pixel's centre.
//
// the host suite pins the float expressions here and in sd_draw bit for
// bit, built with -ffp-contract=off (test/host/Makefile): never reorder or
// regroup them. a board's own compiler flags may fuse multiply-adds, so a
// board build is not held to the pins: a sample on a rounding edge may land
// on the neighbouring pixel.
struct SD_Xform {
  float a, b, c, d, tx, ty;

  // a turn by angle (radians, clockwise on the panel), mirrored in x first
  // when flipX, then a move that lands the pivot on (x, y). a sprite pixel
  // (sx, sy) with pivot (px, py) goes to dx = sx - px (negated when flipX),
  // dy = sy - py, then
  //     x' = x + cos(angle) dx - sin(angle) dy
  //     y' = y + sin(angle) dx + cos(angle) dy
  static SD_Xform rigid(float x, float y, float angle, bool flipX = false);

  // rigid with the sprite pixels scaled about the pivot first: a, b, c, d
  // are rigid's times scale, so scale 1 is rigid bit for bit. a scale of 0
  // or below, or nan, is taken as 0: a zero linear part, which sd_draw
  // refuses.
  static SD_Xform scaled(float x, float y, float angle, bool flipX, float scale);

  // child first, then parent: compose(p, q).apply(v) is p.apply(q.apply(v)).
  static SD_Xform compose(const SD_Xform &parent, const SD_Xform &child);

  // the inverse matrix, or the zero matrix when there is no finite inverse.
  SD_Xform inverse() const;

  void apply(float x, float y, float *ox, float *oy) const {
    *ox = a * x + b * y + tx;
    *oy = c * x + d * y + ty;
  }
};

#endif  // SD_XFORM_H
