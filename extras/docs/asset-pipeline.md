# Asset pipeline: art to header

SecondDimension draws sprites compiled into your sketch as C++ headers. The
pipeline is one dependency-free Python script:

```
Aseprite / any pixel editor
        │  File > Export, .png
        ▼
tools/png2sprite.py
        │  emits a sprite-format-1 C++ header: ink, mask, pivot
        ▼
#include "Arm.h"  →  sd_draw(target, arm, SD_Xform::rigid(x, y, angle));
```

## Quick start

```sh
python3 tools/png2sprite.py arm.png -o Arm.h --name arm
```

Copy `Arm.h` next to your sketch and use it:

```cpp
#include <SecondDimension.h>
#include "Arm.h"

// the arm's pivot pixel lands on (64, 32), turned 0.3 rad clockwise
sd_draw(canvas, arm, SD_Xform::rigid(64, 32, 0.3f));
```

`--name arm` sets the symbol: the file emits `static const SD_Sprite arm`
over `arm_ink[]` and `arm_mask[]`, so several sprites coexist in one sketch
with no macros to collide on. Without `--name`, the sprite is named for its
file stem, made into a C identifier (`upper-arm.png` → `upper_arm`).

Several PNGs go into one header, each named for its file:

```sh
python3 tools/png2sprite.py head.png eye.png mouth.png -o Face.h
```

`--name` takes exactly one PNG; with several it is refused, since the names
come from the files.

## From Aseprite

Draw on a transparent background in any colours you like: the converter
reads each pixel as one of three things (below), so a palette of white,
black and transparent shows exactly what you will get. Mark **one pixel**
where the sprite turns (a shoulder, an elbow, a mallet's grip) in a pivot
colour chosen by what that pixel should be:

| pivot pixel | the pixel paints |
|---|---|
| exactly `#ff00ff` (magenta), alpha 255 | white (ink) |
| exactly `#800080` (purple), alpha 255 | black |
| either colour, alpha 1..254 | nothing (transparent) |

So a pivot on a white outline is magenta, one inside a black fill is
purple, and one in empty space beside the shape is either at partial
alpha: the artist never loses the pixel under the pivot. Then File > Export
(or Export As) to PNG at 100% scale. RGBA and 8-bit indexed PNGs are both
read; if an indexed export is refused for its bit depth, export from RGB
colour mode (Sprite > Color Mode > RGB Color) instead.

## Pieces drawn as a group

A piece drawn by `sd_drawGroup` (several sprites drawn as one shape, an arm of
segments and joints, say) is drawn **solid**: its mask is the whole shape and
its ink is interior detail only, never the outline. The group derives one
outline from the union of its pieces' shapes (conventions.md, R8), so two
pieces that overlap show no line between them. In Aseprite: fill the shape in
black, draw any crease or finger line inside it in white, and leave the edge
black: the edge is drawn for you. End a piece round where it meets another at
a joint, and give the joint a round disc of its own, so the contour stays
smooth as the pieces turn.

A piece drawn with its own white outline still converts and draws, but in a
group that outline becomes interior detail wherever another piece covers it.

### Outlined art: `--silhouette`

An artist may draw a group piece solid, as above, or draw it the usual way,
a closed white outline on an opaque black background with details inside,
and convert it with `--silhouette`:

```sh
python3 tools/png2sprite.py art/*.png --silhouette art/head.png -o Body.h
```

The option names a PNG among those given (repeat it for several); the
others convert as always. That PNG is read as below, and then:

1. **The background is unpainted:** every pixel that reads as the top-left
   pixel does (black here) and is 4-connected to the image's border
   through pixels that read so. One that is not, the inside of a closed
   outline, stays painted black.
2. **The outline is the shape's edge, not detail:** every ink pixel with a
   4-neighbour unpainted after step 1, or outside the image, is painted
   black (mask only). That is the ring the group derives, so the piece
   drawn alone gives back the drawing's outline pixel for pixel, and
   another piece over it leaves no line. One ring is peeled, never a
   second: an outline pixel that touches the background only at a corner
   (where the outline turns inward) stays ink, and the group draws it as
   it was drawn.

The pivot pixel paints as its colour says, in neither step. A PNG whose
top-left pixel is unpainted (alpha 0) already has its background unpainted
and converts as it would without the option. The outline must be closed:
a gap lets the background in, and a PNG left with nothing inside an
outline is refused. The banner and the printout say what the option did.

## What the converter does (the contract)

Stable across tool versions; the host suite pins it.

1. **Decodes** the PNG with its own reader (Python's `zlib` does the
   inflating): bit depth 8; colour types grey, RGB, indexed (with or
   without `tRNS`), grey+alpha and RGBA; all five row filters; not
   interlaced. Every chunk's CRC is checked.
2. **Reads** each pixel, in this order, frozen:
   - alpha 0: not painted (mask 0), whatever its colour.
   - exactly `#ff00ff` or `#800080`: the pivot. One per file, of any kind;
     a second is an error. At alpha 255, `#ff00ff` is painted white (mask
     1, ink 1) and `#800080` black (mask 1, ink 0); at alpha 1..254 either
     is not painted (mask 0). Any other colour, however near, is not a
     pivot.
   - otherwise luminance `0.299 r + 0.587 g + 0.114 b` (a grey pixel is its
     own grey) at or above 128: ink, painted white (mask 1, ink 1).
   - under 128: painted black (mask 1, ink 0).

   Any alpha but 0 counts as painted; the colour alone decides black or
   white. Luminance is computed in integers (`299 r + 587 g + 114 b >=
   128000`), so it is exact: grey 127 is black, 128 is ink, and grey stored
   as RGB reads the same as grey stored as grey.
3. **Places the pivot** at the pivot pixel, or, with none, at the
   sprite's centre `(w / 2, h / 2)` in integer division.
4. **Packs** ink and mask into Adafruit_GFX `drawBitmap`'s row layout:
   `(w + 7) / 8` bytes per row, rows top to bottom, the leftmost pixel in
   the most significant bit. An unturned sprite drawn by `sd_draw` and the
   same planes drawn by `drawBitmap` (mask in black, then ink in white) are
   the same pixels.
5. **Emits** a self-contained header: a banner (the tool, the format
   number, and per sprite its source file, size and pivot, and where the
   pivot came from and how its pixel paints), an include guard named for
   the output file (`Arm.h` → `ARM_H`), `#include <SD_Sprite.h>`, a
   `static_assert` on `kSDSpriteFormat` that turns a stale header into a
   readable compile error, and per sprite the two `static const uint8_t
   … SD_PROGMEM` planes and the `static const SD_Sprite` initializer. The
   header is written only after every PNG has converted.
6. **Prints** one line per sprite: its file, size, pivot and where the
   pivot came from, and how many pixels it paints, ink and black.

## What is refused

Each with a message naming the file and the reason, a non-zero exit, and no
header written, rather than a misread:

- bit depths other than 8 (16-bit channels, or 1/2/4-bit palettes and
  greys): export 8 bits per channel;
- interlaced (Adam7) PNGs: export without interlacing;
- a `tRNS` colour key on a grey or RGB PNG: export with an alpha channel;
- an unknown critical chunk, a bad CRC, a truncated file, a missing
  palette, a palette index past the palette;
- a second pivot pixel (`#ff00ff` or `#800080` at any alpha but 0);
- a side over 32767 pixels (`SD_Sprite` sizes are `int16_t`);
- under `--silhouette`, nothing left inside an outline (a gap in it), and
  a `--silhouette` PNG that is not among those given.

Ancillary chunks (text, gamma, colour profiles and the like) are skipped.

## The format

Sprite format 1 (`kSDSpriteFormat` in `SD_Sprite.h`) is frozen:

```cpp
struct SD_Sprite {
  int16_t w, h;             // pixels
  int16_t pivotX, pivotY;   // the pixel the transform's origin sits on
  const uint8_t *ink;       // 1 bpp, rows padded to bytes, msb first
  const uint8_t *mask;      // same layout; 1 = this pixel is painted
};
```

Headers initialize it positionally, so its fields are never reordered or
retyped; a new layout is a new struct and a new format number, and every
header made for format 1 then fails its `static_assert` until it is
regenerated.

## Where the art lives

The planes are `static const … SD_PROGMEM`. `SD_PROGMEM` is the library's
flash-placement macro, in `SD_Config.h`:

- **Teensy:** `PROGMEM`. Teensy 4's linker would otherwise copy const data
  into RAM (DTCM) at boot; with it the planes stay in flash and the blit
  reads them there through plain pointers (the flash is memory-mapped, so
  there is no `pgm_read_*` anywhere).
- **Any other board, and the host:** empty, so the planes go wherever that
  toolchain puts const data.

The `SD_Sprite` view beside them is plain `static const`, so on Teensy 4
the view itself sits in RAM. Their sizes are in [memory.md](memory.md).

## Multiple sprites, multiple headers

- **Include each sprite header from exactly ONE translation unit.** The
  arrays are `static const`, so including a header from N `.cpp` files
  duplicates its planes N times.
- Each header includes `<SD_Sprite.h>` itself, so it compiles whatever the
  include order and whether `SecondDimension.h` is present.
- The header is committed with its art: regenerate it whenever the PNG
  changes.

## Checking a conversion

`sd_validateSprite(arm)` checks the view (both planes present, sizes at
least 1, the pivot inside the sprite). Converter output always passes;
`sd_draw` runs the same check on every call and draws nothing, returning
false, for a sprite that fails it.

## The test fixture

`test/host/fixtures/arm.png` (the outlined arm, its pivot a `#ff00ff`
pixel on the shoulder's outline at `(0, 3)`, painted white) converts to the
pinned `test/host/fixtures/arm_expected.h`; regenerate it from the
repository root with exactly:

```sh
python3 tools/png2sprite.py test/host/fixtures/arm.png \
    -o test/host/fixtures/arm_expected.h --name arm
```

The host suite runs the same command and compares byte for byte, so a
converter change that moves the header moves a pin: regenerate it
deliberately.
