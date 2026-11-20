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

// a group: several sprites drawn as one shape, with one 1 px outline derived
// from the union of their shapes, so no line shows where pieces overlap. a
// piece's mask is its solid shape and its ink is interior detail only; the
// outline is not in the art (extras/docs/conventions.md, R8).

// sets every scratch pixel sd_draw would plot: the shape. never clears;
// returns what sd_draw returns.
bool sd_drawSilhouette(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                       SD_Sample sample = SD_SAMPLE_2x2);

// sets every scratch pixel sd_draw would plot lit: the ink. never clears;
// returns what sd_draw returns.
bool sd_drawInk(SD_Canvas &scratch, const SD_Sprite &sprite, const SD_Xform &xform,
                SD_Sample sample = SD_SAMPLE_2x2);

// stamps the box [x0, x1) x [y0, y1). the shape is the silhouette's set
// pixels and its pinholes (an unset pixel whose four neighbours are set). a
// shape pixel with a 4-neighbour outside the shape, or outside the canvas,
// plots lit; any other shape pixel plots lit where the ink canvas is set,
// else dark; a pixel outside the shape is left alone. never plots outside
// the target.
void sd_stampOutline(SD_Target &target, const SD_Canvas &silhouette, const SD_Canvas &ink, int x0,
                     int y0, int x1, int y1);

// draws n bones of a posed rig as one group, each bone's sprite through its
// world transform. table is the bone array the rig was built over (the rig
// does not hand out its bones), and bones lists n indices into it. a
// listed bone's visible flag is not read: mark a group's bones invisible so
// the rig's own draw leaves them to the group. silhouette and ink are two
// scratch canvases the target's size; only the group's box (grown by two
// pixels) is cleared and drawn in them, so groups drawn one after another
// can share them. never allocates.
//
// returns false, drawing nothing, for a null pointer, a negative count or a
// listed bone the rig has not posed (or outside the rig). returns false,
// leaving that piece out, for a piece sd_draw refuses (a bad sprite, or a
// bone of scale 0 or below); true otherwise.
bool sd_drawGroup(SD_Target &target, const SD_Rig &rig, const SD_Bone *table, const int *bones,
                  int n, SD_Canvas &silhouette, SD_Canvas &ink, SD_Sample sample = SD_SAMPLE_2x2);

#endif  // SD_GROUP_H
