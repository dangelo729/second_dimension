# Coordinate & rig conventions

Eight rules, fixed. Everything in the library — the blit, the transform, the
rig, the IK, the golden test suite — assumes exactly these. Each rule ends
with what breaks if you assume otherwise, because that is how convention
bugs present.

## R1 — The frame is GFX's: x right, y down, positive angles clockwise

Every coordinate is a target pixel in Adafruit_GFX's frame: origin top-left,
+x right, +y down. Angles are radians, and a positive angle turns clockwise
on the panel. `SD_Xform::rigid(x, y, angle)` maps a pivot-relative sprite
pixel `(dx, dy)` to

```
x' = x + cos(angle)·dx − sin(angle)·dy
y' = y + sin(angle)·dx + cos(angle)·dy
```

the standard rotation matrix, read in a y-down frame. At +90° the sprite's +x
points down the panel. The sprites, the bones and `sd_ik2` all share this one
frame; there is no second convention anywhere.

*If you assume otherwise:* the y-up habit from maths (or from a 3D world,
such as ThirdDimension's) turns every angle the wrong way and mirrors every
point you hand over. Flip y once, where a y-up world's numbers enter, and
nowhere else.

## R2 — Integer coordinates name pixel centres

Pixel `(x, y)` covers `[x − 0.5, x + 0.5] × [y − 0.5, y + 0.5]`, on the
target and in a sprite alike. `sd_draw` carries each target pixel's centre
(or its four sample points at ±0.25 px) back into the sprite and rounds to
the nearest sprite pixel with `floorf(u + 0.5f)`. So a sprite at angle 0 at
an integer position reproduces the art pixel for pixel, and at 90, 180 and
270 degrees it is the art turned, exactly.

*If you assume otherwise:* treating integers as pixel corners puts every
part half a pixel off, and an unturned sprite placed at a half-integer
position samples on pixel edges, so rows and columns double up or drop out.

## R3 — The pivot is a pixel, and the transform's origin sits on its centre

`SD_Sprite::pivotX, pivotY` names one pixel of the sprite. A transform maps
sprite pixels *relative to the pivot*: `sd_draw` subtracts the pivot before
the matrix, so `rigid(x, y, angle)` lands the pivot pixel's centre on
`(x, y)` and turns the sprite about it. In the art, the pivot is one pixel
in a pivot colour: exactly `#ff00ff` at alpha 255 (painted white), exactly
`#800080` at alpha 255 (painted black), or either at alpha 1..254 (not
painted). With none, it is `(w / 2, h / 2)`.

*If you assume otherwise:* placing a sprite by its top-left corner, as
`drawBitmap` does, puts it off by its pivot; turning it then swings it about
a point you did not mean.

## R4 — A bone's (ox, oy) is in its parent's sprite pixels, from the parent's pivot

For a root bone, `(ox, oy)` is its pivot's position on the target. For a
child, it is where the child's pivot sits in the parent's sprite, in that
sprite's pixels, measured from the parent's pivot: a bone pinned at its
parent's pivot has `ox = oy = 0`, and one pinned at the far end of a 16 px
arm whose pivot is its pixel `(0, 3)` has `ox = 15, oy = 0` (pixel centre to
pixel centre). A bone's `angle` turns it about its own pivot, in its
parent's frame, so angles add down a chain. The posed transform is

```
world[root]  = rigid(ox, oy, angle, flipX)
world[child] = compose(world[parent], rigid(ox, oy, angle, flipX))
```

and `world(i).apply(0, 0, …)` is where bone `i`'s pivot landed.

*If you assume otherwise:* measuring `(ox, oy)` from the parent's top-left
corner shifts every child by the parent's pivot, and a joint on the art's
outline lands a few pixels inside or outside it, turning with the parent so
the error moves.

## R5 — A mirror applies first, about the pivot, and carries down the chain

`flipX` mirrors the bone's own sprite about its pivot (x negated) *before*
the bone's own angle turns it, so a mirrored root still turns clockwise for
a positive angle: at angle 0 it points the other way. Everything posed under
a mirrored bone is posed in the mirrored frame, as `compose` gives it:

- a child's `ox` flips with its parent (`ox = +6` lands 6 px the other way
  on the panel);
- a child's sprite is drawn mirrored, and its `angle` turns the other way on
  the panel (anticlockwise for a positive angle);
- a child that is itself `flipX` under a mirrored parent is unmirrored.

One `flipX`, on a joint above a chain or on the chain's first bone, mirrors
the whole chain.

*If you assume otherwise:* setting `flipX` on every bone of a mirrored arm
cancels the mirror on every other bone, and the arm comes apart at
alternate joints; and expecting a mirrored chain's child angles to keep
turning clockwise bends its elbow the wrong way.

## R6 — Two-bone IK works in the frame its chain hangs in

`sd_ik2(l1, l2, tx, ty, bendSign, a1, a2)` solves a chain from the origin:
link `l1` at angle `a1`, then link `l2` at `a2` relative to the first, so the
tip is at `(l1 cos a1 + l2 cos(a1 + a2), l1 sin a1 + l2 sin(a1 + a2))` in
R1's convention. `a1` is the chain's first bone's angle and `a2` its child's,
the child pinned at `(l1, 0)` in the first bone's sprite. The target must be
given in the frame the first bone's angle is measured in — its parent's:

- for a root, the panel target minus the root's position;
- under a joint, the panel target through the inverse of the joint's world
  transform, `world(joint).inverse().apply(px, py, &tx, &ty)`; for an
  unturned mirrored joint at `(sx, sy)` that is `(sx − px, py − sy)`.

`bendSign` picks the elbow: positive (or zero) the one at `a2 ≥ 0`, which in
an unmirrored frame sits anticlockwise of the line from the shoulder to the
target (above it when reaching right); negative the other. Out of reach, the
chain points straight at the target with `a2 = 0`; closer than
`|l1 − l2|`, it folds (`a2 = ±π`) with its tip on the ray toward the target;
at the origin, `a1` is kept and `a2` folds. Each of those returns false; a
reached target returns true. The links run along each bone's +x: mirror a
joint above the chain rather than its first link, whose mirror would send
link 1 along −x where `sd_ik2` does not look.

*If you assume otherwise:* handing a chain under a turned or mirrored joint
its panel target aims it somewhere else entirely, and reading `bendSign` as
"elbow up" holds only until the chain reaches left or is mirrored.

## R7 — Draw order is array order

Bones are listed parents first; `sd_validateRig` and `pose()` refuse a bone
whose parent is not −1 or an index below its own (`pose()` then leaves that
bone and every one after it unposed, at the zero matrix, which `sd_draw`
refuses, so they are not drawn). `draw()` blits every visible bone with a
sprite in array order, so a later bone paints over an earlier one — its
black pixels too: a black-filled limb hides what is behind it. A pure joint
(`sprite == nullptr`) and an invisible bone are posed and never drawn; their
children are drawn as usual. There is no depth and no layer field. A part
that must sit in front of something drawn between the rig's passes is
marked invisible for `draw()` and drawn afterwards with `sd_draw` through
`world(i)`.

*If you assume otherwise:* listing a child before its parent is not
reordered for you — the rig refuses it and stops drawing there; and
expecting a parent to cover its children (or a later root to sit behind an
earlier one) gets the opposite.

## R8 — A group's outline is derived from the union of its pieces

`sd_drawGroup` draws a group of bones as one shape. Each listed bone's sprite
goes into two scratch canvases through its posed world transform, by
`sd_draw`'s own sampling: its mask into the silhouette (every pixel any
masked sample lands on), its ink into the ink canvas (every pixel a masked
sample with ink lands on). Then the group's box is stamped onto the target.
The shape is the silhouette's set pixels and its pinholes: an unset pixel
whose four neighbours are all set, which pieces sampled apart can leave
where their edges meet (in the crook of a bent elbow, say), is the shape's,
not a hole. A shape pixel with any of its four neighbours outside the shape
is the outline and plots lit; any other shape pixel plots lit where the ink
canvas has it and dark elsewhere; a pixel outside the shape is never
plotted. A neighbour outside the canvas is outside the shape, so a shape at
the canvas's edge is outlined there; a hole of two pixels or more is
outlined round its rim. The scratch canvases are in the target's pixels and
caller-owned; the box is every pixel the listed bones' `sd_draw` could
visit, both canvases are cleared over it grown by two, and it is stamped
grown by one. The outline is 1 px wide whatever the sampling and whatever
the angle, and closed: it is the edge of a set of pixels, not a stroke that
was turned.

So a group piece's art is its solid shape: the mask is the shape, and the ink
is interior detail only (a crease, finger lines). Where pieces overlap, the
union has no edge, so no line shows there. Joints read smooth at any angle
when the pieces meeting there end round and a disc covers the joint.

A group draws its bones whatever their `visible` flag says, and the rig's
`draw()` still draws every visible bone; mark a group's bones invisible and
draw the group after `draw()` (the R7 pattern).

*If you assume otherwise:* drawing a piece with its own outline puts that
outline inside the shape wherever another piece covers it, which is exactly
the seam a group exists to remove; handing the group canvases smaller than
the target clips the group to them; and listing a group's bones as visible
draws them twice, once outlined each on its own.
