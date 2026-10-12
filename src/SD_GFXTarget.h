// SPDX-License-Identifier: MIT
#ifndef SD_GFXTARGET_H
#define SD_GFXTARGET_H

// the adafruit_gfx adapter. reached through SecondDimension_GFX.h, which
// includes Adafruit_GFX.h first: that umbrella is the one header in the
// library that includes gfx, so the core builds without it.
#ifndef SECONDDIMENSION_GFX_H
#error "include <SecondDimension_GFX.h>, not SD_GFXTarget.h"
#endif

#include <stdint.h>

#include "SD_Config.h"
#include "SD_Target.h"

// sprites on any adafruit_gfx display through drawPixel: a lit plot draws
// the lit colour, a dark plot the dark one. lit and dark default to 1 and 0,
// every mono driver's white and black; on a colour panel any two colours.
//
// width() and height() are read from the gfx object on every call, so the
// target can be built before display.begin() and follows setRotation().
class SD_GFXTarget : public SD_Target {
 public:
  explicit SD_GFXTarget(Adafruit_GFX &gfx, uint16_t lit = 1, uint16_t dark = 0)
      : gfx_(gfx), lit_(lit), dark_(dark) {}

  int16_t width() const override { return gfx_.width(); }
  int16_t height() const override { return gfx_.height(); }
  void plot(int16_t x, int16_t y, bool on) override { gfx_.drawPixel(x, y, on ? lit_ : dark_); }

  void setColors(uint16_t lit, uint16_t dark) {
    lit_ = lit;
    dark_ = dark;
  }

 private:
  Adafruit_GFX &gfx_;
  uint16_t lit_, dark_;
};

#endif  // SD_GFXTARGET_H
