// SPDX-License-Identifier: MIT
//
// 02_Group: 01_Arm's arm with its upper arm and forearm drawn as a group,
// one shape with one outline, so no line shows where they overlap at the
// elbow. a group piece's art is solid (the group derives the outline), so
// art/ holds 01_Arm's two arm pieces filled in.
//
// needs the adafruit gfx and adafruit ssd1306 libraries. arm_art.h is made
// from art/ by, from the repository root:
//   python3 tools/png2sprite.py examples/02_Group/art/*.png -o examples/02_Group/arm_art.h

#include <Adafruit_SSD1306.h>
#include <SecondDimension_GFX.h>
#include "arm_art.h"  // upper_arm and forearm (solid), mallet

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SD_GFXTarget target(display);

// the group's bones are invisible, so the rig's own draw leaves them to it
SD_Bone bones[] = {
  // parent  ox     oy     angle  flipX  visible  sprite
  {-1,       30.0f, 32.0f, 0.0f,  false, false,   &upper_arm},  // in the group
  {0,        17.0f, 0.0f,  0.0f,  false, false,   &forearm},    // in the group
  {1,        16.0f, 0.0f,  0.0f,  false, true,    &mallet},     // drawn by the rig
};
SD_Xform world[3];
SD_Rig arm(bones, world, 3);

// the group's bones, and the two scratch canvases it is drawn through
const int armGroup[] = {0, 1};
uint8_t shapeBuf[SD_Canvas::bytesFor(128, 64)], inkBuf[SD_Canvas::bytesFor(128, 64)];
SD_Canvas shape(shapeBuf, 128, 64), ink(inkBuf, 128, 64);

void setup() { display.begin(SSD1306_SWITCHCAPVCC, 0x3C); }

void loop() {
  float t = millis() * 0.001f;  // the mallet head, 10 px past the hand, goes round a circle
  float tx = 60.0f + 12.0f * cosf(t) - bones[0].ox;  // in the shoulder's frame
  float ty = 32.0f + 12.0f * sinf(t) - bones[0].oy;
  sd_ik2(17.0f, 16.0f + 10.0f, tx, ty, 1.0f, bones[0].angle, bones[1].angle);
  arm.pose();
  display.clearDisplay();
  sd_drawGroup(target, arm, armGroup, 2, shape, ink);  // the arm, one outline
  arm.draw(target);  // the visible bones: the mallet, over the arm
  display.display();
}
