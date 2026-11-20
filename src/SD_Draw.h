// SPDX-License-Identifier: MIT
#ifndef SD_DRAW_H
#define SD_DRAW_H

#include "SD_Config.h"
#include "SD_Sprite.h"
#include "SD_Target.h"
#include "SD_Xform.h"

// how many points of each target pixel ask the sprite what it is.
enum SD_Sample {
  SD_SAMPLE_1,    // one, at the pixel centre: nearest neighbour
  SD_SAMPLE_2x2,  // four, at (+-0.25, +-0.25): keeps 1 px lines whole
};

// draws a sprite through a transform that maps its pivot-relative pixels
// to target pixels (SD_Xform). every target pixel in the sprite's turned box,
// clipped to the target, carries its sample points back into the sprite: if
// any lands on a masked pixel the target pixel is plotted, lit if any of
// those is ink, else dark; if none does, the pixel is left alone.
//
// never allocates, never reads outside the sprite, never plots outside the
// target. returns false, drawing nothing, for a sprite that fails
// sd_validateSprite, a matrix with no finite inverse or an unknown sample
// mode; true otherwise, even when nothing lands on the target.
bool sd_draw(SD_Target &target, const SD_Sprite &sprite, const SD_Xform &xform,
             SD_Sample sample = SD_SAMPLE_2x2);

#endif  // SD_DRAW_H
