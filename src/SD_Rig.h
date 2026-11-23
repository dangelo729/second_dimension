// SPDX-License-Identifier: MIT
#ifndef SD_RIG_H
#define SD_RIG_H

#include <stdint.h>

#include "SD_Config.h"
#include "SD_Draw.h"
#include "SD_Sprite.h"
#include "SD_Target.h"
#include "SD_Xform.h"

class SD_Canvas;

// one bone of a rig: a sprite, or a pure joint with none, hung from its
// parent and scaled and turned about its own pivot. a plain aggregate, so a
// rig is an initializer table, parents first. scale may be left out of a
// table, which means 1; its default makes SD_Bone need c++14.
//
// (ox, oy) is where the bone's pivot sits: for a root, in target pixels; for
// a child, in the parent's sprite pixels from the parent's pivot. a mirror
// or a scale carries down to every bone under it (extras/docs/conventions.md,
// R4, R5, R9).
struct SD_Bone {
  int8_t parent;            // -1 for a root; always less than its own index
  float ox, oy;             // its pivot in the parent's sprite pixels
  float angle;              // radians, about its pivot, in the parent's frame
  bool flipX;               // mirrored about its pivot
  bool visible;
  const SD_Sprite *sprite;  // may be null: a pure joint
  float scale = 1.0f;       // its sprite pixels about its pivot, before the mirror and the turn
};

// the static check pose() makes as it goes: bones non-null when count > 0,
// count not negative, and every bone's parent -1 or an index below its own.
bool sd_validateRig(const SD_Bone *bones, int count);

// poses a caller-owned bone array into a caller-owned array of world
// transforms of the same count; it owns nothing and never allocates. a
// bone's world transform is SD_Xform::scaled(ox, oy, angle, flipX, scale)
// for a root and compose(its parent's world, that) for a child, so it maps
// the bone's pivot-relative sprite pixels to target pixels, as sd_draw
// takes it. a bone of scale 0 or below, and every bone under it, collapses
// onto its pivot and is never drawn.
class SD_Rig {
 public:
  // every world transform starts as the zero matrix, which sd_draw refuses,
  // so a bone never posed is never drawn. a negative count is taken as zero.
  SD_Rig(const SD_Bone *bones, SD_Xform *world, int count);

  // walks the bones once, in array order, writing each world transform.
  // returns false, leaving the bad bone and every bone after it at the zero
  // matrix, when a bone's parent is not -1 or an index below its own; the
  // bones before it are posed. a null array with a count above zero poses
  // nothing and returns false.
  bool pose();

  // draws every visible bone with a sprite, in array order, through sd_draw
  // with its posed world transform: later bones paint over earlier ones. a
  // pure joint (null sprite) and an invisible bone are posed, never drawn;
  // their children are drawn as usual.
  void draw(SD_Target &target, SD_Sample sample = SD_SAMPLE_2x2) const;

  // a bone's posed world transform: world(i).apply(0, 0, ...) is where its
  // pivot landed. an index outside [0, count) reads the zero matrix.
  const SD_Xform &world(int i) const;

 private:
  // the group reads the bones' sprites and which bones pose() has placed.
  friend bool sd_drawGroup(SD_Target &, const SD_Rig &, const int *, int, SD_Canvas &, SD_Canvas &,
                           SD_Sample);

  const SD_Bone *bones_;
  SD_Xform *world_;
  int count_;
  int posed_;  // bones [0, posed_) are posed
};

#endif  // SD_RIG_H
