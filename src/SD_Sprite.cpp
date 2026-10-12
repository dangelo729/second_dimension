// SPDX-License-Identifier: MIT

#include "SD_Sprite.h"

// performs, in order: both planes present, both sizes at least 1, the pivot
// inside the sprite. returns false on the first violation.
bool sd_validateSprite(const SD_Sprite &s) {
  if (s.ink == nullptr || s.mask == nullptr) return false;
  if (s.w < 1 || s.h < 1) return false;
  if (s.pivotX < 0 || s.pivotX >= s.w) return false;
  if (s.pivotY < 0 || s.pivotY >= s.h) return false;
  return true;
}
