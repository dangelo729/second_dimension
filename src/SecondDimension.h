// SPDX-License-Identifier: MIT
//
// SecondDimension: a tiny articulated-sprite library for 32-bit
// microcontrollers, in the spirit of adafruit gfx.
//
// this umbrella includes the dependency-free core: the sprite format, the
// transform, the target seam with its 1-bit canvas, the blit, the rig of
// bones and the two-bone ik. the core needs nothing beyond the c standard
// headers (and arduino.h on teensy, for SD_PROGMEM), which is what lets the
// full test suite run on a host pc. for an adafruit_gfx display include
// SecondDimension_GFX.h instead.
#ifndef SECONDDIMENSION_H
#define SECONDDIMENSION_H

#include "SD_Config.h"
#include "SD_Sprite.h"
#include "SD_Xform.h"
#include "SD_Target.h"
#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Rig.h"
#include "SD_IK.h"

#endif  // SECONDDIMENSION_H
