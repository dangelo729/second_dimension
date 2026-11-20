// SPDX-License-Identifier: MIT
//
// SecondDimension: articulated 1-bit sprites for 32-bit microcontrollers.
//
// the core, with no dependency: the sprite, the transform, the target and
// its 1-bit canvas, the blit, the rig, the two-bone ik and the group. it
// needs only the c standard headers (and arduino.h on teensy, for
// SD_PROGMEM), so it also builds on a host pc. for an adafruit_gfx display
// include SecondDimension_GFX.h instead.
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
#include "SD_Group.h"

#endif  // SECONDDIMENSION_H
