# Memory and cost

Nothing in the library allocates: sprites are views over const arrays, and
rigs and canvases are arrays the sketch owns. Sizes are for a 32-bit board.

| What | Size |
|---|---|
| a sprite's two planes ([where they live](asset-pipeline.md#where-the-art-lives)) | `2 x (w + 7) / 8 x h` bytes: 256 B at 32x32 |
| an `SD_Sprite` view | 16 B |
| an `SD_Bone` | 28 B |
| an `SD_Xform`, one per bone | 24 B |
| an `SD_Rig` | 12 B |
| an `SD_Canvas` buffer | `(w + 7) / 8 x h` bytes: 1 KB at 128x64 |

So twenty bones and their transforms are about 1 KB.

## Time

- `sd_draw` visits every pixel of the sprite's turned box, clipped to the
  target, and reads up to four samples in each (one under `SD_SAMPLE_1`). A
  20x20 sprite at 45 degrees has a box of about 30x30, 900 pixels.
- `pose()` is one `SD_Xform::scaled` and one `compose` per bone.
- `sd_drawGroup` is two `sd_draw`s per bone, one for the shape and one for
  the ink, then a clear and a stamp of the group's box. Its two scratch
  canvases are the target's size: 1 KB each at 128x64.
