// SPDX-License-Identifier: MIT
#ifndef SD_RIG_H
#define SD_RIG_H

#include <stdint.h>

#include "SD_Config.h"
#include "SD_Draw.h"
#include "SD_Sprite.h"
#include "SD_Target.h"
#include "SD_Xform.h"

// one bone of a rig: a sprite (or a pure joint) hung from its parent at a
// point, scaled and turned about its own pivot. a plain aggregate, so a rig
// is written as an initializer table, parents first; the scale is last and
// may be left out, which means 1:
//
//     SD_Bone bones[] = {
//       // parent  ox     oy    angle  flipX  visible  sprite      scale
//       {-1,       40.0f, 32.0f, 0.0f, false, true,    &upper_arm},
//       { 0,       20.0f,  0.0f, 0.0f, false, true,    &forearm,   0.5f},
//     };
//
// (ox, oy) is where this bone's pivot sits: for a root, in target pixels;
// for a child, in the parent's sprite pixels relative to the parent's pivot,
// so a bone pinned at its parent's pivot has ox = oy = 0. angle turns the
// bone about its pivot, in the parent's frame (radians, clockwise on the
// panel when nothing above it is mirrored). scale is a uniform scale of the
// bone's sprite pixels about its pivot, and so of everything under it.
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

// a pose of a caller-owned bone array into a caller-owned array of world
// transforms of the same count. the rig owns nothing and never allocates;
// it reads the bones and writes only the world array.
//
// a bone's world transform is its local transform, SD_Xform::scaled(ox, oy,
// angle, flipX, scale), for a root and compose(parent's world, its local
// transform) for a child, so it maps the bone's pivot-relative sprite pixels
// to target pixels, as sd_draw takes it. the local transform scales the
// sprite pixels first, then mirrors, turns and moves them; at scale 1 it is
// rigid(ox, oy, angle, flipX) bit for bit.
//
// scaling follows from compose too: a scaled bone scales everything below
// it. a child's (ox, oy) is in its parent's sprite pixels, so under a parent
// of scale 0.5 a child pinned 15 px out lands 7.5 px out on the panel, and
// it is drawn at half its size; a child's own scale multiplies its
// parent's. world(i).apply(0, 0, ...) is still where the bone's pivot
// landed. a bone of scale 0 or below (or nan) is posed with a zero linear
// part at its pivot, and every bone below it collapses onto that point:
// sd_draw refuses them, so none of them is drawn.
//
// mirroring follows from compose: a mirrored bone mirrors everything below
// it. a child's ox flips with its parent (ox = +6 under a mirrored parent
// lands 6 px the other way on the panel), its sprite is drawn mirrored, and
// its angle turns the other way on the panel (anticlockwise for a positive
// angle). a child that is itself flipX under a mirrored parent is drawn
// unmirrored. so one flipX on the first bone of a chain mirrors the chain.
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
  const SD_Bone *bones_;
  SD_Xform *world_;
  int count_;
};

#endif  // SD_RIG_H
