// SPDX-License-Identifier: MIT
#ifndef SD_IK_H
#define SD_IK_H

#include "SD_Config.h"

// two-bone inverse kinematics, closed form. a chain from the origin of its
// own frame: link l1 at angle a1, then link l2 at a2 relative to the first,
// so its tip is at
//     (l1 cos a1 + l2 cos(a1 + a2),  l1 sin a1 + l2 sin(a1 + a2))
// in the convention of SD_Xform::rigid: x right, y down, a positive angle
// clockwise on the panel. a1 is a root bone's angle and a2 its child's, the
// child pinned at the end of the first link. the target (tx, ty) is in the
// same frame: subtract the shoulder's position first.
//
// the elbow comes from the law of cosines, its cosine clamped to [-1, 1].
// bendSign picks which of the two elbows: positive (or zero) the one at
// a2 >= 0, negative the one at a2 <= 0. with d the target's distance:
//
//   |l1 - l2| <= d <= l1 + l2   reached: the tip on the target, true.
//   d > l1 + l2                 out of reach: straight at the target,
//                               a1 = atan2(ty, tx), a2 = 0, false.
//   0 < d < |l1 - l2|           too close: folded (a2 = +-pi by bendSign),
//                               the tip on the ray toward the target at
//                               |l1 - l2|, false. a1 = atan2(ty, tx) when
//                               l1 >= l2; when l2 is the longer, a1 points
//                               the other way, so the tip still points at
//                               the target.
//   d == 0                      a1 kept as passed in, a2 folded, false.
//
// a link length that is not finite and above zero, or a target that is not
// finite, returns false with both angles left as passed in. a2 falls in
// [-pi, pi]; a1 is not wrapped and falls in (-2 pi, 2 pi).
bool sd_ik2(float l1, float l2, float tx, float ty, float bendSign, float &a1, float &a2);

#endif  // SD_IK_H
