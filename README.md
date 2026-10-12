# SecondDimension

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**Articulated 1-bit sprites for 32-bit microcontrollers**: your own pixel art, drawn at
any angle about its pivot and posed as bones, on small OLED and TFT displays, in the
spirit of Adafruit GFX and the shape of its sibling ThirdDimension.

> **Status: pre-release (v0.3.0).** The sprite format, the transform, the target seam
> with its 1-bit canvas, the blit, the PNG converter, the rig of bones with a scale on
> each, the two-bone IK, groups drawn as one shape with a derived outline, and the
> Adafruit_GFX adapter exist;
> the host suite passes with bit-exact golden pins, and the example sketch compiles for
> Teensy 4.1. Nothing has run on a board yet: no frame has been seen on a panel and no
> time has been measured.

---

## What it is / what it is not

**It is:**

- One blit, `sd_draw`, that draws a 1-bit sprite with a mask through a 2x3 affine: any
  angle, any pivot, mirrored or not. Every target pixel in the sprite's turned box is
  carried back into the sprite and asks it what it is, at four sub-pixel points by
  default (`SD_SAMPLE_2x2`), so a 1 px outline stays whole when it turns, or at one
  (`SD_SAMPLE_1`, nearest neighbour).
- A rig of bones, `SD_Rig` over a caller-owned `SD_Bone[]`: each bone a sprite (or a
  pure joint) pinned in its parent's sprite and scaled and turned about its own pivot,
  posed parents first and drawn in array order, later over earlier. Mirror one bone and
  the chain under it mirrors; scale one and the chain under it scales.
- A closed-form two-bone IK, `sd_ik2`, that aims an arm's two angles at a point, either
  elbow, and says whether it reached.
- Groups, `sd_drawGroup`: several bones drawn as one shape, its one 1 px outline derived
  from the union of their shapes, so an arm of segments and round joints reads as one
  continuous arm at any angle, with no line where its pieces overlap. Two caller-owned
  scratch canvases hold the shape and its interior ink while it is stamped.
- A sprite format in Adafruit_GFX `drawBitmap`'s row layout: two planes, ink and mask,
  so an unturned sprite is exactly what `drawBitmap` draws, and black pixels can cover
  what is under them.
- An asset pipeline: Aseprite (or any editor) → PNG → `tools/png2sprite.py` → a header
  of flash-resident arrays. The pivot is one pixel in a pivot colour in the art.
- A target seam, `SD_Target`, of three methods; a 1-bit canvas target in the core; and
  an opt-in Adafruit_GFX adapter, `SD_GFXTarget`.
- Allocation-free: sprites are views over const arrays, canvases and rigs are
  caller-owned.
- Host-tested, with bit-exact golden frame hashes and a pinned converter output.

**It is not:**

- A display driver: the library never touches hardware; you push the pixels yourself.
- Colour or animated (yet): no colour targets, sprite sheets, frames, clips or tweens.
  A bone has a uniform scale; the blit takes any affine, so a stretch or a shear is a
  matter of the matrix when it comes.
- A scene graph: no layers and no depth. Draw order is array order within a rig and
  call order between draws.
- For AVR. A 32-bit MCU with hardware float is the floor (Teensy 4.x, ESP32, STM32F4+,
  RP2350); every header enforces it with a compile-time `#error`.

---

## Quick start

The shape of [`examples/01_Arm`](examples/01_Arm/01_Arm.ino): three sprites converted
from PNGs, posed as an arm, its mallet aimed by IK, drawn on an SSD1306.

```cpp
#include <Adafruit_SSD1306.h>
#include <SecondDimension_GFX.h>
#include "arm_art.h"  // python3 tools/png2sprite.py art/*.png -o arm_art.h

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SD_GFXTarget target(display);

SD_Bone bones[] = {
    // parent  ox      oy     angle flipX  visible sprite
    {-1,       30.0f,  32.0f, 0.0f, false, true,   &upper_arm},  // the shoulder
    {0,        17.0f,  0.0f,  0.0f, false, true,   &forearm},    // the elbow
    {1,        16.0f,  0.0f,  0.0f, false, true,   &mallet},     // the hand
};
SD_Xform world[3];
SD_Rig arm(bones, world, 3);

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop() {
  // the mallet head round a circle, in the shoulder's frame
  float t = (float)millis() * 0.001f;
  float tx = 60.0f + 12.0f * cosf(t * 1.5707963f) - bones[0].ox;
  float ty = 32.0f + 12.0f * sinf(t * 1.5707963f) - bones[0].oy;
  sd_ik2(17.0f, 16.0f + 10.0f, tx, ty, 1.0f, bones[0].angle, bones[1].angle);
  arm.pose();

  display.clearDisplay();
  arm.draw(target);  // upper arm, forearm, mallet: later over earlier
  display.display();
}
```

No `begin()` on any `SD_` object; all angles are radians, clockwise on the panel.

Include exactly one umbrella header per sketch:

- `<SecondDimension.h>`: the dependency-free core (the sprite, the transform, the target
  seam, the canvas, the blit, the rig and the IK).
- `<SecondDimension_GFX.h>`: the core **plus** `SD_GFXTarget`; requires the *Adafruit
  GFX Library* (the library itself declares no hard dependency on it).

---

## Core concepts

```
PNG (Aseprite export, one pivot pixel)
  └─ tools/png2sprite.py   ink + mask + pivot → header (planes in flash)
       └─ SD_Sprite        a non-owning view over two const planes, frozen format 1
            └─ SD_Bone[]   parent · (ox, oy) in the parent's sprite · angle · flipX · scale
                 └─ SD_Rig     pose(): world[i] = compose(world[parent], scaled(...))
                      └─ sd_draw    turned box → inverse map → 2x2 OR sampling
                           └─ SD_Target   plot(x, y, on)  ← the seam
```

- **The frame is GFX's.** x right, y down, angles in radians, and a positive angle
  turns clockwise on the panel. Integer coordinates name pixel centres: the pivot
  pixel's centre lands exactly on the position you give, and angle 0 at an integer
  position reproduces the art pixel for pixel.
- **A pixel is one of three things.** A masked pixel with its ink bit set paints white
  (lit), a masked pixel without it paints black (dark), an unmasked pixel is left
  alone. Black is a write: a sprite's black pixels cover whatever was there.
- **One loop draws everything.** The sprite's pixel area goes through the matrix by its
  four corners; their bounding box, clipped to the target, is every pixel visited.
  Each pixel's sample points go back through the inverse into the sprite: any masked
  sample paints the pixel, white if any of them is ink. Rotation, mirroring and the
  pivot are all the matrix; the loop never changes.
- **2x2 is the default for a reason.** Nearest neighbour breaks every 1 px outline at
  most angles; four samples OR'd keep it whole, at the cost of a little extra weight
  on a turned line. At 0, 90, 180 and 270 degrees both draw exactly the art, turned.
- **A bone hangs from its parent's sprite.** A child's `(ox, oy)` is where its pivot
  sits in the parent's sprite pixels, from the parent's pivot (a bone pinned at its
  parent's pivot has `ox = oy = 0`); a root's is its position on the target. Its
  `angle` turns it about its own pivot in the parent's frame, so angles add down a
  chain. `rig.world(i)` is bone `i`'s posed transform: where its pivot landed, for a
  part drawn in a second pass.
- **Mirroring carries down.** `flipX` mirrors a bone about its pivot before its own
  turn, and everything under it: a child's `ox` flips with its parent and its angle
  turns the other way on the panel. One `flipX` mirrors a chain.
- **A bone has a scale.** `SD_Bone`'s last member, `scale`, is a uniform scale of the
  bone's sprite pixels about its pivot, applied before its mirror and its turn
  (`SD_Xform::scaled(x, y, angle, flipX, scale)`, which at scale 1 is `rigid` bit for
  bit). It defaults to 1, so a table of seven-field bones means scale 1 throughout (the
  default member makes `SD_Bone` need C++14 or later). It carries down: a child's
  `(ox, oy)` is in its parent's sprite pixels, so under a parent of scale 0.5 a child
  pinned 15 px out lands 7.5 px out and is drawn at half its size, and a child's own
  scale multiplies its parent's. A scale of 0 or below is a zero linear part at the
  pivot: that bone and everything under it are posed onto the pivot and never drawn.
- **What a scale does to 1 px ink.** The blit carries each target pixel back into the
  sprite, so a scale is sampled, not redrawn. Under `SD_SAMPLE_1`, shrinking skips
  every sprite row and column that falls between two target pixel centres (one in four
  at 0.75), so a 1 px line breaks where a skipped row or column crossed it, and growing
  doubles some rows and columns (a 1 px line grown to 2 is 2 px wide). Under
  `SD_SAMPLE_2x2` the four samples, half a target pixel apart, catch every sprite row
  and column down to a scale of 0.5, so lines stay whole; the trade is weight: at any
  scale but 1 the samples straddle the art's pixels and every line is drawn a little
  bolder (a 1 px line grown to 2 is 3 px wide). A group's outline is derived after the
  scale, from the union of the scaled shapes, so it is 1 px wide and closed at any
  scale.
- **Parents first, drawn in order.** A bone's parent is −1 or an earlier index;
  `sd_validateRig` and `pose()` refuse anything else. `draw()` blits in array order, so
  later bones paint over earlier ones. A bone with no sprite is a pure joint: posed,
  never drawn.
- **A group is one shape.** `sd_drawGroup(target, rig, table, bones, n, silhouette, ink)`
  draws the listed bones' masks into a silhouette canvas and their ink into an ink
  canvas (both `sd_draw`'s own sampling, via `sd_drawSilhouette` and `sd_drawInk`), then
  `sd_stampOutline` plots the silhouette's edge lit and its inside dark but for the ink.
  A group piece's art is solid: the mask is the shape, the ink interior detail only.
  Mark a group's bones invisible so `draw()` leaves them to the group.
- **IK aims a chain in its own frame.** `sd_ik2(l1, l2, tx, ty, bendSign, a1, a2)` gives
  the chain's first bone's angle and its child's, for a target in the frame the chain
  hangs in (the panel minus the shoulder, for a root); out of reach it points straight
  at the target, too close it folds, and it returns whether it reached.
- **`sd_draw` and `pose()` return `bool`**: false for a sprite that fails
  `sd_validateSprite`, a matrix with no finite inverse, or a rig with a bone listed
  before its parent.
- **Deeper reference.** The frame, pixel centres, the pivot, the bone frame, mirroring,
  the IK convention and draw order, with what breaks if you guess, are in
  [`extras/docs/conventions.md`](extras/docs/conventions.md).

## Render targets

| Target | Header | Output | Notes |
|---|---|---|---|
| `SD_Canvas` | `SecondDimension.h` | 1-bit into a caller-owned buffer | `drawBitmap`'s row layout, `(w + 7) / 8` bytes per row, msb first; `clear`, `pixel` read-back |
| `SD_GFXTarget` | `SecondDimension_GFX.h` | any Adafruit_GFX display via `drawPixel` | reads `width()/height()` from the display on every call; lit/dark colours default to 1/0, `setColors(lit, dark)` |
| your own | subclass `SD_Target` | anything | 3 overrides: `width`, `height`, `plot` |

```cpp
class MyTarget : public SD_Target {
 public:
  int16_t width() const override { return 128; }
  int16_t height() const override { return 64; }
  void plot(int16_t x, int16_t y, bool on) override { myDisplay.drawPixel(x, y, on); }
};
```

The contract: `sd_draw` never calls `plot` outside `[0, width()) x [0, height())`, so a
target may skip bounds checks; a dark plot (`on == false`) is a write, never a skip;
and a pixel the sprite leaves unpainted is never plotted, so the target keeps whatever
it held there.

## Bring in your own art

```
python3 tools/png2sprite.py arm.png -o Arm.h --name arm
python3 tools/png2sprite.py head.png eye.png -o Face.h   # several, each named for its file
```

```cpp
#include "Arm.h"
sd_draw(canvas, arm, SD_Xform::rigid(x, y, angle));
```

The converter is Python 3.9+ with nothing but the standard library: its own PNG reader
(8-bit grey, RGB, indexed, grey+alpha and RGBA, all five filters), a frozen reading
(transparent is unpainted, light is white, dark is black), and a header that
`static_assert`s the sprite format. The pivot is one pixel in a pivot colour chosen by
what that pixel should be: `#ff00ff` paints it white, `#800080` black, and either at
alpha 1..254 leaves it unpainted, so a pivot on the art never costs a pixel. Odd files
(16-bit, interlaced, an unknown critical chunk, two pivots) are refused with a message
rather than misread. The full contract is in
[`extras/docs/asset-pipeline.md`](extras/docs/asset-pipeline.md).

## Examples

| Sketch | Shows |
|---|---|
| `01_Arm` | an arm of three converted sprites posed as a rig, its mallet head aimed round a circle by `sd_ik2`, through `SD_GFXTarget` on an SSD1306 |

## Memory and time

- A sprite is its two planes, `2 x (w + 7) / 8 x h` bytes (a 32x32 sprite is 256 B),
  plus a 16 B view. The converter marks the planes `SD_PROGMEM`: `PROGMEM` on Teensy
  4.x, whose linker would otherwise copy const data into RAM at boot, and nothing on
  ESP32, STM32 and RP2350, which keep const data in flash natively. Either way the
  planes cost flash, not RAM, and the blit reads them through plain pointers. The view
  itself is plain `static const` (in RAM on Teensy 4.x).
- A rig is caller-owned: its bones (28 B each on 32-bit targets) and its posed
  transforms (24 B each), plus 12 B for the `SD_Rig`. Twenty bones are about 1 KB.
- A canvas is `(w + 7) / 8 x h` bytes, caller-owned: 1 KB at 128x64.
- `sd_draw` costs the turned box's pixels times the samples: a 20x20 sprite at any
  angle is about 900 pixels, 3600 samples; `pose()` is one matrix compose per bone.
  A group costs two `sd_draw`s a bone (shape and ink), and a clear and a stamp of its
  box; its two scratch canvases are caller-owned, 1 KB each at 128x64.
  Nothing has been measured on a board.

## Testing

The core needs no Arduino headers off the board (on Teensy it includes `Arduino.h` only
for `PROGMEM`), so the whole suite runs on a host PC:

```
make -C test/host run
```

It pins bit-exact FNV-1a frame hashes of three fixture sprites at 0, 20, 45, 70, 90
and 180 degrees and mirrored, and at scales 0.5, 0.75, 0.9, 1.1, 1.5 and 2, in both
samplings, of an arm-arm-mallet rig at five poses and scaled four ways, and of a group
(two segments and a round elbow disc drawn as one shape) at six poses, and at each
through a root of scale 0.9 and 1.1; checks that a group's silhouette and ink are
`sd_draw`'s own pixels, that overlapping pieces give one contour with no line inside,
that a scaled group is one shape with one 1 px outline, and that nothing outside the
shape is touched; checks that a scaled sprite's pixels grow with the scale, that a 1 px
line shrunk to 0.75 breaks under `SD_SAMPLE_1` and stays whole under `SD_SAMPLE_2x2`,
and that `scaled` at scale 1 is `rigid` bit for bit; reproduces the scratch mockup the
2x2 default came from; fuzzes clipping against a brute-force pass over exactly-sized
heap planes; checks the rig against a pose worked by hand, its refusals, its draw
order, its pure joints, its mirroring and its scales (a bone of scale 0 never drawn);
checks
`sd_ik2` by forward kinematics over a grid of targets, both elbows, out of reach and too
close; and runs the converter on fixture PNGs against a pinned header. The pins require
`-ffp-contract=off` (FMA contraction moves last-ulp results). Pins at angles other than
the quarter turns go through the platform's `cosf`/`sinf` and are verified on macOS
(Apple clang, arm64) only; the 0, 90 and 180 degree pins and the structural checks do
not depend on the libm. The suite must stay green at -O1, -O2 and under ASan/UBSan:

```
make -C test/host clean && make -C test/host run CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -ffp-contract=off -fsanitize=address,undefined"
```

The Adafruit_GFX adapter has no host test (it needs Adafruit_GFX); compiling the example
for a board is its check:

```
pio ci --board teensy41 --lib . -O "lib_deps=adafruit/Adafruit SSD1306@^2.5.9, adafruit/Adafruit GFX Library@^1.11.9, adafruit/Adafruit BusIO@^1.15.0" examples/01_Arm
```

## Versioning

SemVer, with two domain rules: any change visible in the golden-pinned output is **at
least a MINOR** bump (with a CHANGELOG entry explaining why the pins moved), and any
`SD_Sprite` layout change is a **new format number and a MAJOR** bump (generated
headers check the format at compile time). During 0.x, MINOR releases may break API,
called out under a "Breaking" heading.

## License & acknowledgments

MIT, see [LICENSE](LICENSE).

The target seam follows **Adafruit_GFX**: one small abstraction, legible limits. No
Adafruit code is copied; the sprite planes share `drawBitmap`'s row layout so the two
interoperate.
