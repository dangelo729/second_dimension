# Changelog

All notable changes to SecondDimension are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Before 1.0, a minor release may change what an existing sketch must say or what it
draws; each such change is listed under "Breaking". A change to the sprite layout is a
new sprite format number, which a header made for the old one refuses at compile time.

## [0.3.0] - 2026-11-23

The first release.

### Added

- **Sprites** (`SD_Sprite`): 1-bit pixel art with a mask and a pivot pixel, in Adafruit
  GFX's `drawBitmap` row layout, kept in flash on Teensy.
- **The converter** (`tools/png2sprite.py`, Python 3.9 or later, nothing to install)
  turns PNGs into a sprite header: transparent pixels are left out, light ones draw
  white and dark ones black, and one `#ff00ff` or `#800080` pixel marks the pivot
  (drawn white or black; with none, the pivot is the centre). `--silhouette` turns an
  outline drawn on an opaque background into a group piece. A PNG it cannot read
  exactly is refused with a message. It comes with the PlatformIO package.
- **Drawing at any angle** (`sd_draw`, `SD_Xform::rigid`, `SD_Xform::scaled`): a sprite
  turned about its pivot, mirrored and scaled. Four samples a pixel by default
  (`SD_SAMPLE_2x2`) keep 1 px lines whole; `SD_SAMPLE_1` is nearest neighbour. It never
  allocates, and never reads outside the sprite or draws outside the target.
- **Rigs** (`SD_Bone`, `SD_Rig`): a table of bones, parents first, each hung from its
  parent's sprite with its own angle, mirror and scale; `pose()` places them and
  `draw()` draws them in order.
- **Two-bone IK** (`sd_ik2`): the angles that put a two-bone chain's tip on a point,
  with either elbow.
- **Groups** (`sd_drawGroup`): several bones of a rig drawn as one shape with one 1 px
  outline, so no line shows where the pieces overlap. A group piece's art is solid; the
  outline is drawn for it.
- **Targets**: `SD_Canvas`, a 1-bit buffer; `SD_GFXTarget`, any Adafruit GFX display
  (`SecondDimension_GFX.h`); or a subclass of `SD_Target` for anything else.
- **Examples**: `01_Arm`, an arm of three sprites aimed by IK, and `02_Group`, the same
  arm with its two segments drawn as one group, on an SSD1306.
- **Tests**: a host suite (`make -C test/host run`) that checks the drawn pixels bit
  for bit.

It needs a 32-bit board and C++14 or later; AVR and C++11 builds stop with a message
saying so.
