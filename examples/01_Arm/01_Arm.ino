// SPDX-License-Identifier: MIT
//
// 01_Arm: an arm of three sprites posed as a rig of bones, its mallet head
// aimed round a circle by two-bone ik, on an ssd1306 128x64 i2c oled.
//
// needs the adafruit gfx and adafruit ssd1306 libraries. arm_art.h is made
// from art/ by, from the repository root:
//   python3 tools/png2sprite.py examples/01_Arm/art/*.png -o examples/01_Arm/arm_art.h

#include <Adafruit_SSD1306.h>
#include <SecondDimension_GFX.h>
#include "arm_art.h"  // upper_arm, forearm, mallet

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SD_GFXTarget target(display);

SD_Bone bones[] = {
  // parent  ox     oy     angle  flipX  visible  sprite
  {-1,       30.0f, 32.0f, 0.0f,  false, true,    &upper_arm},  // shoulder, on the panel
  {0,        17.0f, 0.0f,  0.0f,  false, true,    &forearm},    // elbow, on the upper arm
  {1,        16.0f, 0.0f,  0.0f,  false, true,    &mallet},     // hand, on the forearm
};
SD_Xform world[3];
SD_Rig arm(bones, world, 3);

void setup() { display.begin(SSD1306_SWITCHCAPVCC, 0x3C); }

void loop() {
  float t = millis() * 0.001f;  // the mallet head, 10 px past the hand, goes round a circle
  float tx = 60.0f + 12.0f * cosf(t) - bones[0].ox;  // in the shoulder's frame
  float ty = 32.0f + 12.0f * sinf(t) - bones[0].oy;
  sd_ik2(17.0f, 16.0f + 10.0f, tx, ty, 1.0f, bones[0].angle, bones[1].angle);
  arm.pose();
  display.clearDisplay();
  arm.draw(target);
  display.display();
}
