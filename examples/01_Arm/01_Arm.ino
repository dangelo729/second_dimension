// SPDX-License-Identifier: MIT
//
// 01_Arm: the minimal SecondDimension sketch. an arm of three sprites (an
// upper arm, a forearm and a mallet, converted from the pngs in art/) posed
// as a rig of bones, its mallet head tracing a slow circle by two-bone ik,
// drawn onto an ssd1306 128x64 i2c oled through the adafruit_gfx adapter.
//
// regenerate the sprite header from the repository root with:
//   python3 tools/png2sprite.py examples/01_Arm/art/*.png -o examples/01_Arm/arm_art.h
//
// requires: the adafruit gfx and adafruit ssd1306 libraries (install via the
// library manager). wiring: standard i2c (sda/scl), display at address 0x3c.

#include <Adafruit_SSD1306.h>
#include <SecondDimension_GFX.h>

#include "arm_art.h"  // defines: upper_arm, forearm, mallet (planes in flash)

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SD_GFXTarget target(display);  // plots sprites through drawPixel, lit 1, dark 0

// link lengths from the art, pivot to pin, in sprite pixels
const float kUpperArm = 17.0f;  // upper_arm's pivot (3, 3) to its elbow (20, 3)
const float kForearm = 16.0f;   // forearm's pivot (2, 2) to its grip (18, 2)
const float kMallet = 10.0f;    // mallet's pivot (0, 4) to its head (10, 4)

// the arm, parents first and drawn in this order: each child's (ox, oy) is
// where its pivot sits in its parent's sprite, from the parent's pivot.
SD_Bone bones[] = {
    // parent  ox         oy     angle  flipX  visible  sprite
    {-1,       30.0f,     32.0f, 0.0f,  false, true,    &upper_arm},  // the shoulder
    {0,        kUpperArm, 0.0f,  0.0f,  false, true,    &forearm},    // the elbow
    {1,        kForearm,  0.0f,  0.0f,  false, true,    &mallet},     // the hand
};
SD_Xform world[3];  // the posed transforms, one per bone
SD_Rig arm(bones, world, 3);

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop() {
  // the mallet head's target: a circle of 12 px about (60, 32), once every 4 s
  float t = (float)millis() * 0.001f;  // seconds
  float tx = 60.0f + 12.0f * cosf(t * 1.5707963f);
  float ty = 32.0f + 12.0f * sinf(t * 1.5707963f);

  // two links from the shoulder: the upper arm, then the forearm and the
  // mallet held in line as one. aimed in the shoulder's frame, elbow up.
  sd_ik2(kUpperArm, kForearm + kMallet, tx - bones[0].ox, ty - bones[0].oy, 1.0f,
         bones[0].angle, bones[1].angle);
  arm.pose();  // parents first: each bone's world transform

  display.clearDisplay();
  arm.draw(target);   // upper arm, forearm, mallet: later over earlier
  display.display();  // push the gfx buffer to the panel
}
