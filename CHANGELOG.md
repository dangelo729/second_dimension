# Changelog

All notable changes to SecondDimension are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) with
two domain rules on top: any change visible in the golden-pinned output (the frame
hashes or the converter's pinned header) is at least a MINOR bump, with an entry here
explaining why the pins moved, and any change to the `SD_Sprite` layout is a new sprite
format number plus a MAJOR bump. During 0.x, MINOR releases may break API; breaks are
called out under a "Breaking" heading.

## [0.2.0] - unreleased

### Added

- **Groups: one shape, one derived outline** (`SD_Group.h`). Several sprites drawn as
  one shape: an arm of segments and round joints reads as one continuous arm at any
  angle, with no line where its pieces overlap. `sd_drawSilhouette(scratch, sprite,
  xform, sample)` sets every pixel `sd_draw` would plot (the solid shape, ink
  ignored) and `sd_drawInk` every pixel it would plot lit; both are `sd_draw` itself,
  through a scratch canvas, so the sampling is the blit's own.
  `sd_stampOutline(target, silhouette, ink, x0, y0, x1, y1)` stamps a box. The shape
  is the silhouette and its pinholes (an unset pixel whose four neighbours are all
  set: pieces sampled apart leave one where their edges meet, as in the crook of an
  elbow). A shape pixel with any 4-neighbour outside the shape (outside the canvas
  counts as outside) plots lit, any other shape pixel lit where the ink canvas has it
  and dark elsewhere, and a pixel outside the shape is left alone; a hole of two
  pixels or more is outlined. `sd_drawGroup(target,
  rig, table, bones, n, silhouette, ink, sample)` clears both caller-owned scratch
  canvases over the group's box, draws every listed bone's silhouette and ink through
  its posed world transform, and stamps; the bone table it reads sprites from is the
  one the rig was built over (the rig has no read access to its bones), and a listed
  bone's `visible` flag is not read, so a bone drawn by its group is marked invisible
  for the rig's own draw. Nothing allocates; `SD_Bone`, `SD_Rig` and the draw path are
  unchanged.
- **The art convention for a group piece** (`extras/docs/asset-pipeline.md`,
  `extras/docs/conventions.md` R8): the mask is the solid shape, the ink interior
  detail only; the outline is derived.
- **The group suite** (`test/host/test_group.cpp`): the silhouette and the ink are
  `sd_draw`'s plotted and lit pixels for every fixture at the golden poses; a lone
  rectangle is a 1 px outline round a dark inside; two overlapping rectangles give one
  contour with no line inside; interior ink shows; a shape at the canvas's edge is
  outlined there; a pinhole is filled and a two-pixel hole outlined; nothing outside
  the shape is touched on a dark, a lit or a checker background; no plot falls outside the target; the refusals; and golden
  frames of a two-segment arm with a round elbow disc at six poses in both samplings,
  new pins. No existing pin moved.

## [0.1.0] - unreleased

### Added

- **The sprite, format 1: `SD_Sprite`.** A non-owning view over two 1-bit planes, ink
  and mask, in Adafruit_GFX `drawBitmap`'s row layout (`(w + 7) / 8` bytes per row,
  msb first), with a pivot pixel. A masked pixel paints white where its ink bit is set
  and black where it is clear; an unmasked one is not painted. `sd_validateSprite`
  checks the view (both planes present, sizes at least 1, the pivot inside the
  sprite); `sd_spritePixel` reads one pixel, and anything outside the sprite reads
  unmasked without touching memory. `kSDSpriteFormat` is frozen at 1.
- **The transform: `SD_Xform`.** A 2x3 affine, target pixels from pivot-relative
  sprite pixels, in GFX's frame (x right, y down, positive angles clockwise on the
  panel). `rigid(x, y, angle, flipX)`, `compose(parent, child)` (child first),
  `inverse()` (all zeros for a matrix with no finite inverse) and `apply`.
- **The target seam: `SD_Target`** (`width`, `height`, `plot`), with its contract:
  `sd_draw` never plots outside the target, a dark plot is a write, an unpainted
  pixel is never plotted. **`SD_Canvas`**, a 1-bit target over a caller-owned buffer
  in `drawBitmap`'s layout, with `clear`, `pixel` read-back and the raw buffer.
- **The blit: `sd_draw(target, sprite, xform, sample)`.** The sprite's pixel area goes
  through the matrix by its four corners; every target pixel of that box, clipped to
  the target, carries its sample points back through the inverse into the sprite and
  reads it there: any masked sample paints the pixel, white if any masked sample's ink
  is set, else black. `SD_SAMPLE_2x2` (four samples at ±0.25 px, the default) keeps
  1 px outlines whole under a turn; `SD_SAMPLE_1` is nearest neighbour. Never
  allocates, never reads outside the sprite, never plots outside the target; returns
  false for a bad sprite or a matrix with no finite inverse.
- **The converter: `tools/png2sprite.py`.** Python 3.9+, standard library only, with
  its own PNG reader (8-bit grey, RGB, indexed with or without `tRNS`, grey+alpha and
  RGBA; all five row filters; not interlaced). The reading is frozen: alpha 0 is not
  painted, luminance at or above 128 is ink, under is black, and one pivot pixel per
  file: exactly `#ff00ff` at alpha 255 is the pivot painted white, exactly `#800080` at
  alpha 255 the pivot painted black, either at alpha 1..254 the pivot left unpainted;
  none puts the pivot at the centre. The planes are emitted `SD_PROGMEM`. Several PNGs
  go into one header, each named for its file. 16-bit, sub-byte, interlaced,
  colour-keyed grey or RGB, unknown-critical-chunk and two-pivot files are refused
  with a message.
- **Flash placement: `SD_PROGMEM`** in `SD_Config.h`, `PROGMEM` on Teensy (whose
  linker otherwise copies const data into RAM at boot) and empty elsewhere, as
  ThirdDimension's `TD_PROGMEM` is.
- **The rig: `SD_Bone`, `SD_Rig`, `sd_validateRig`.** A bone is a plain aggregate
  (parent, pivot offset `(ox, oy)` in the parent's sprite pixels from the parent's
  pivot, angle, `flipX`, visible, sprite or null for a pure joint), so a rig is an
  initializer table. `SD_Rig` over a caller-owned bone array and a caller-owned array
  of world transforms: `pose()` walks the bones once, parents first, and refuses a bone
  whose parent is not -1 or an earlier index (that bone and those after it stay at the
  zero matrix and are not drawn); `draw(target, sample)` blits every visible bone with
  a sprite in array order, later over earlier; `world(i)` reads a bone's posed
  transform. A mirrored bone mirrors everything under it.
- **Two-bone IK: `sd_ik2(l1, l2, tx, ty, bendSign, a1, a2)`.** The closed-form elbow by
  the law of cosines in the transform's convention, either elbow by `bendSign`; out of
  reach points the chain straight at the target, too close folds it toward the target,
  the origin keeps `a1`; returns whether the target was reached.
- **The Adafruit_GFX adapter: `SD_GFXTarget`** in `SecondDimension_GFX.h`, header-only:
  plots through `drawPixel` with settable lit and dark colours (1 and 0 by default),
  reading the display's size on every call. The core never includes Adafruit_GFX.
- **The example: `examples/01_Arm`.** An arm of three sprites converted from PNGs in
  its `art/`, posed as a rig, its mallet head tracing a circle by `sd_ik2`, through
  `SD_GFXTarget` on an SSD1306 128x64. Compiles for Teensy 4.1; not yet run on a
  board.
- **The host suite** (`make -C test/host run`), the acceptance gate: format, mask,
  transform, identity against a `drawBitmap`-style copy, quarter turns, golden frame
  hashes of three fixture sprites at 0, 20, 45, 70, 90 and 180 degrees and mirrored in
  both samplings, the scratch mockup's frame, clipping (half off each edge, wholly off,
  a 1x1 target, and a fuzz against a brute-force pass) over exactly-sized heap planes,
  the rig (a chain posed by hand, the refusals, draw order, pure joints, mirroring, and
  golden frames of an arm-arm-mallet chain at five poses in both samplings), the IK
  (forward kinematics over a grid of targets, both elbows, the reach's edges, out of
  reach, too close, the origin, bad inputs, and chains posed through a rig), and the
  converter (a pinned header for `arm.png`, the same planes from every colour type,
  the three pivot readings, the frozen reading pixel by pixel, the refusals). Green at
  -O1, -O2 and under ASan/UBSan with `-ffp-contract=off`.
- Packaging: `library.properties` / `library.json` manifests, `keywords.txt`, MIT
  license, `extras/docs/asset-pipeline.md`, `extras/docs/conventions.md`.

### Changed (before release; pins moved, no version bump)

- **The pivot pixel paints.** It was read and then left unpainted, which punched a
  hole wherever the pivot sat on the art. Now `#ff00ff` at alpha 255 paints white,
  `#800080` at alpha 255 paints black, and either at alpha 1..254 paints nothing. The
  `arm.png` fixture's pivot is back on its outline at `(0, 3)`, so the pinned
  `arm_expected.h` moved (pivot `(1, 3)` to `(0, 3)` in its initializer and banner,
  planes unchanged) and the
  converted arm draws through plain `rigid` at the mockup's angles. `reading.png` was
  redrawn with one pivot (it held two magenta pixels, which now both read as pivots)
  and a third row of near-pivot colours, so its pinned planes moved to ink `35 49 02`,
  mask `ff 6f 7f`. The other arm fixtures carrying the pivot were regenerated with it
  at `(0, 3)`; three fixtures were added for the black and unpainted pivots. No frame
  hash moved.
- **The planes are placed in flash on Teensy.** The converter emits `SD_PROGMEM` on
  both planes, so the pinned `arm_expected.h` moved (the two plane declarations).
