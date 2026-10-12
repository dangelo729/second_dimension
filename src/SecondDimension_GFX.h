// SPDX-License-Identifier: MIT
//
// the core plus SD_GFXTarget, the adafruit_gfx adapter. include this header
// (not SecondDimension.h) in sketches that draw to a gfx display; it needs
// the "Adafruit GFX Library" installed.
//
// header-only and never included by the core umbrella: no .cpp under src/
// references a gfx symbol, so the core compiles the same with or without gfx
// (and with no arduino headers at all on a host pc). the include below is
// also what lets the arduino builder and platformio find the gfx dependency
// for sketches that opt in.
#ifndef SECONDDIMENSION_GFX_H
#define SECONDDIMENSION_GFX_H

#include <Adafruit_GFX.h>

#include "SecondDimension.h"
#include "SD_GFXTarget.h"

#endif  // SECONDDIMENSION_GFX_H
