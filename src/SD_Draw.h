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

// draws a sprite through a transform (see SD_Xform: the matrix maps sprite
// pixels minus the pivot to target pixels).
//
// the box: the sprite's pixel area [-0.5, w - 0.5] x [-0.5, h - 0.5], minus
// the pivot, goes through the matrix by its four corners; their bounding box,
// floored and ceiled, clipped to the target, is every pixel visited. each
// visited pixel centre (px, py) is offset by each sample point, carried back
// through the inverse matrix into sprite space, plus the pivot, rounded to
// the nearest pixel with floorf(u + 0.5f), and read. if any sample lands on
// a masked pixel the target pixel is plotted, lit if any masked sample's ink
// is set, else dark; if none does, the pixel is left alone.
//
// never allocates, never reads outside the sprite's planes, never plots
// outside the target. returns false, drawing nothing, for a sprite that
// fails sd_validateSprite, a matrix with no finite inverse, or an unknown
// sample mode; true otherwise, even when nothing lands on the target.
bool sd_draw(SD_Target &target, const SD_Sprite &sprite, const SD_Xform &xform,
             SD_Sample sample = SD_SAMPLE_2x2);

#endif  // SD_DRAW_H
