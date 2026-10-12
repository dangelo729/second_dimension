// SPDX-License-Identifier: MIT

#include "SD_Rig.h"

static const SD_Xform kZero = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

bool sd_validateRig(const SD_Bone *bones, int count) {
  if (count < 0) return false;
  if (count > 0 && bones == nullptr) return false;
  for (int i = 0; i < count; ++i)
    if (bones[i].parent < -1 || bones[i].parent >= i) return false;
  return true;
}

SD_Rig::SD_Rig(const SD_Bone *bones, SD_Xform *world, int count)
    : bones_(bones), world_(world), count_(count > 0 ? count : 0) {
  if (world_ != nullptr)
    for (int i = 0; i < count_; ++i) world_[i] = kZero;
}

bool SD_Rig::pose() {
  if (count_ > 0 && (bones_ == nullptr || world_ == nullptr)) return false;
  for (int i = 0; i < count_; ++i) {
    const SD_Bone &b = bones_[i];
    if (b.parent < -1 || b.parent >= i) {
      for (int k = i; k < count_; ++k) world_[k] = kZero;
      return false;
    }
    const SD_Xform local = SD_Xform::rigid(b.ox, b.oy, b.angle, b.flipX);
    world_[i] = b.parent < 0 ? local : SD_Xform::compose(world_[b.parent], local);
  }
  return true;
}

void SD_Rig::draw(SD_Target &target, SD_Sample sample) const {
  if (bones_ == nullptr || world_ == nullptr) return;
  for (int i = 0; i < count_; ++i) {
    const SD_Bone &b = bones_[i];
    if (b.visible && b.sprite != nullptr) sd_draw(target, *b.sprite, world_[i], sample);
  }
}

const SD_Xform &SD_Rig::world(int i) const {
  if (world_ == nullptr || i < 0 || i >= count_) return kZero;
  return world_[i];
}
