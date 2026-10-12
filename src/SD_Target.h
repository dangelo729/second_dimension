// SPDX-License-Identifier: MIT
#ifndef SD_TARGET_H
#define SD_TARGET_H

#include <stdint.h>

#include "SD_Config.h"

// the seam. sd_draw works out which target pixels a sprite covers and what
// each one is; the target puts them wherever it puts pixels. three methods
// make a target: width, height, plot.
//
// the contract:
//
//   in-bounds   sd_draw never calls plot outside [0, width()) x [0,
//               height()), so a target may skip bounds checks.
//   two values  on = true is the lit (white, ink) value, on = false the
//               dark (black) one. a dark plot is a write, never a skip: a
//               sprite's black pixels cover what is under them.
//   untouched   a pixel the sprite leaves unpainted is never plotted, so
//               whatever the target already holds there survives.
//
// sd_draw reads width() and height() once per call.
class SD_Target {
 public:
  virtual ~SD_Target() {}

  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  virtual void plot(int16_t x, int16_t y, bool on) = 0;
};

#endif  // SD_TARGET_H
