// SPDX-License-Identifier: MIT
#ifndef SD_GROUP_H
#define SD_GROUP_H

#include <stdint.h>

#include "SD_Canvas.h"
#include "SD_Config.h"
#include "SD_Draw.h"
#include "SD_Rig.h"
#include "SD_Sprite.h"
#include "SD_Target.h"
#include "SD_Xform.h"

// a group: several sprites drawn as one shape, its outline derived from the
// union of their shapes, so where pieces overlap there is no line, only the
// one contour round them all. the pieces are drawn first into two caller-
// owned scratch canvases in the target's pixels (the solid shape, and the
// ink detail inside it), then stamped onto the target: the shape's edge lit,
// its inside dark but for the ink.
//
// a group piece's art: the mask is its solid shape; the ink is interior
// detail only (a crease, finger lines), never the outline. a piece drawn
// with its own outline still works, but its outline is then interior detail
// where pieces overlap.

// the sprite's solid shape into the scratch canvas: every pixel sd_draw would
// plot (any sample on a masked pixel) is set, ink or not. it is sd_draw's
// own sampling, through the canvas: the same box, the same samples, the
// same pixels. only sets, never clears. returns what sd_draw returns.
bool sd_drawSilhouette(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                       SD_Sample sample = SD_SAMPLE_2x2);

// the sprite's ink into the scratch canvas: every pixel sd_draw would plot
// lit (a masked sample with its ink set) is set. only sets, never clears.
bool sd_drawInk(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                SD_Sample sample = SD_SAMPLE_2x2);

// the stamp, over the box [x0, x1) x [y0, y1) clipped to the target and the
// silhouette canvas. the shape is the silhouette's set pixels and its
// pinholes (an unset pixel whose four neighbours are all set: pieces
// sampled apart can leave one where they meet, and it is the shape's, not
// a hole to outline). a shape pixel with any 4-neighbour outside the shape
// plots lit (a neighbour outside the canvas is outside, so a shape at the
// canvas's edge is outlined there); any other shape pixel plots lit where
// the ink canvas has it, else dark; a pixel outside the shape is left
// alone. neighbours are read from the silhouette canvas whether or not they
// lie in the box. never plots outside the target.
void sd_stampOutline(SD_Target &target, const SD_Canvas &silhouette, const SD_Canvas &ink, int x0,
                     int y0, int x1, int y1);

// one group, drawn: the bones listed, indices into table (the bone array the
// rig was built over), each through its posed world transform. the box is
// every pixel any listed bone's sd_draw could visit, clipped to the target
// and both canvases; both scratch canvases are cleared over it grown by two
// pixels, every listed bone with a sprite is drawn into them (its visible
// flag is not read: a bone drawn by its group is marked invisible, so the
// rig's own draw leaves it alone), and the box grown by one is stamped. the
// canvases are in the target's pixels: give them the target's size.
// nothing allocates. returns false, drawing nothing, for a null pointer, a
// negative count, or a listed bone the rig has not posed (an index outside
// the rig reads the zero matrix there, and is refused before the table is
// read); otherwise the group is drawn, and it returns false if any bone's
// sd_draw would (a sprite that fails sd_validateSprite).
bool sd_drawGroup(SD_Target &target, const SD_Rig &rig, const SD_Bone *table, const int *bones,
                  int n, SD_Canvas &silhouette, SD_Canvas &ink, SD_Sample sample = SD_SAMPLE_2x2);

#endif  // SD_GROUP_H
