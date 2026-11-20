# Testing

The core needs no Arduino header off a board, so the whole suite runs on a
host PC, with a C++17 compiler, `make` and Python 3.9 or later (the
converter's tests run `tools/png2sprite.py`):

```sh
make -C test/host run
```

## What it pins

Bit-exact FNV-1a hashes of whole frames, and one file byte for byte:

- `test_draw.cpp`: the three fixture sprites (an arm, a mallet, a flag) at
  0, 20, 45, 70, 90 and 180 degrees and mirrored, and at scales 0.5 to 2,
  in both samplings; and the mockup frame (`test_fixtures.h`), the arm and
  the mallet at four angles in both samplings.
- `test_rig.cpp`: an arm-arm-mallet chain at five poses, and scaled four
  ways.
- `test_group.cpp`: a group of two segments and a round elbow disc at six
  poses, and through a root of scale 0.9 and 1.1.
- `test_convert.cpp`: the converter's header for `fixtures/arm.png`, which
  must equal `fixtures/arm_expected.h`, and the mockup frame drawn with it.

Around the pins it checks what the pins cannot show: clipping against a
brute-force pass over exactly-sized heap planes, the rig against poses
worked by hand, `sd_ik2` by forward kinematics, a group's one outline, and
the refusals.

## What the pins hold for

The pins hold for the host build in `test/host/Makefile`, which compiles
with `-ffp-contract=off`: a compiler free to fuse a multiply and an add into
one instruction rounds some results differently, and a sample on a rounding
edge then lands on the neighbouring pixel. A board built with its own
compiler's flags is not held to the pins. The pins at angles other than 0,
90 and 180 degrees also depend on the C library's `cosf` and `sinf`, and
have been checked on macOS (Apple clang, arm64) only.

## Other builds

The suite stays green at `-O2` and under the address and undefined
behaviour sanitizers:

```sh
make -C test/host clean && make -C test/host run CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -ffp-contract=off -fsanitize=address,undefined"
```

With `SD_FRAMES_DIR=<dir>`, the suite writes every hashed frame there as a
PBM, to look at.

## The examples

The Adafruit GFX adapter has no host test; compiling the examples for a
board checks it, for instance with PlatformIO:

```sh
pio ci --board teensy41 --lib . -O "lib_deps=adafruit/Adafruit SSD1306@^2.5.9, adafruit/Adafruit GFX Library@^1.11.9, adafruit/Adafruit BusIO@^1.15.0" examples/02_Group
```
