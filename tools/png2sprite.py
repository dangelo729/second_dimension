#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""png2sprite.py - png art -> sprite format 1 c++ header.

one-time, host-side, per piece of art:

    python3 tools/png2sprite.py arm.png -o Arm.h --name arm
    python3 tools/png2sprite.py head.png eye.png -o Body.h

what it does:
  1. decodes each png with its own reader (zlib does the inflating): 8-bit
     grey, rgb, indexed (with or without tRNS), grey+alpha and rgba, all five
     row filters, not interlaced. anything else is refused with a message and
     a non-zero exit, never misread: 16-bit or sub-byte depths, interlacing,
     a tRNS colour key on grey or rgb, an unknown critical chunk, a bad crc.
  2. reads every pixel, frozen:
       alpha 0                        not painted (mask 0)
       exactly #ff00ff at alpha 255   the pivot, painted white (mask 1, ink 1)
       exactly #800080 at alpha 255   the pivot, painted black (mask 1, ink 0)
       either, at alpha 1..254        the pivot, not painted (mask 0)
       luminance at or above 128      ink (mask 1, ink 1)
       luminance under 128            black (mask 1, ink 0)
     luminance is 0.299 r + 0.587 g + 0.114 b, grey as itself, computed in
     integers so it is exact. one pivot per file, of any of its three kinds:
     a second is an error, none puts the pivot at (w / 2, h / 2), integer
     division.
  3. packs ink and mask in adafruit_gfx drawBitmap's row layout: (w + 7) / 8
     bytes per row, leftmost pixel in the most significant bit.
  4. writes one header for every png given: an include guard named for the
     output file, #include <SD_Sprite.h>, a kSDSpriteFormat check, and per
     sprite <name>_ink[] and <name>_mask[] (static const, SD_PROGMEM, so they
     stay in flash) and static const SD_Sprite <name>. the header is written
     only after every png has converted.

names: --name sets the symbol when one png is given; with several, each
sprite is named for its file stem, made into a c identifier.
"""
import argparse
import os
import struct
import sys
import zlib

FORMAT = 1
SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}  # colour type -> bytes per pixel at 8 bits
PIVOT_WHITE = (255, 0, 255)  # the pivot, painted white at alpha 255
PIVOT_BLACK = (128, 0, 128)  # the pivot, painted black at alpha 255
MAX_SIDE = 32767  # SD_Sprite sizes are int16_t


class Refused(Exception):
    """a png this tool will not read; the message says why."""


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def unfilter(raw, w, h, bpp):
    """undo the per-row filters; returns h rows of w * bpp bytes."""
    stride = w * bpp
    prev = bytearray(stride)
    rows = []
    at = 0
    for y in range(h):
        kind = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        if kind == 0:
            pass
        elif kind == 1:  # sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif kind == 2:  # up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif kind == 3:  # average
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif kind == 4:  # paeth
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                upleft = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + paeth(left, prev[i], upleft)) & 0xFF
        else:
            raise Refused(f"row {y} has unknown filter type {kind}")
        rows.append(line)
        prev = line
    return rows


def decode_png(data):
    """returns (w, h, pixels) with pixels[y][x] = (r, g, b, a), or raises Refused."""
    if data[:8] != SIGNATURE:
        raise Refused("not a png file (bad signature)")
    at = 8
    header = None
    palette = None
    trns = None
    idat = []
    ended = False
    while at < len(data):
        if at + 8 > len(data):
            raise Refused("truncated chunk header")
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        crc = data[at + 8 + length:at + 12 + length]
        name = kind.decode("latin-1")
        if len(body) != length or len(crc) != 4:
            raise Refused(f"truncated {name} chunk")
        if zlib.crc32(kind + body) & 0xFFFFFFFF != struct.unpack(">I", crc)[0]:
            raise Refused(f"bad crc in {name} chunk")
        at += 12 + length
        if header is None and kind != b"IHDR":
            raise Refused("first chunk is not IHDR")
        if kind == b"IHDR":
            if header is not None or length != 13:
                raise Refused("malformed IHDR chunk")
            header = struct.unpack(">IIBBBBB", body)
            w, h, depth, colour, compression, filtering, interlace = header
            if colour not in CHANNELS:
                raise Refused(f"colour type {colour} is not a png colour type")
            if depth != 8:
                raise Refused(f"{depth}-bit channels are not supported; export 8 bits per channel")
            if interlace != 0:
                raise Refused("interlaced pngs are not supported; export without interlacing")
            if compression != 0 or filtering != 0:
                raise Refused("unknown compression or filter method")
            if w < 1 or h < 1 or w > MAX_SIDE or h > MAX_SIDE:
                raise Refused(f"size {w}x{h} is outside 1..{MAX_SIDE} on a side")
        elif kind == b"PLTE":
            if length == 0 or length % 3 != 0 or length > 768:
                raise Refused("malformed PLTE chunk")
            palette = [tuple(body[i:i + 3]) for i in range(0, length, 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            ended = True
            break
        elif not kind[0] & 0x20:
            raise Refused(f"unknown critical chunk {name}")
        # any other chunk is ancillary and skipped
    if header is None or not ended:
        raise Refused("truncated file (no IEND chunk)")
    w, h, _, colour, _, _, _ = header
    if colour == 3 and palette is None:
        raise Refused("indexed png without a PLTE chunk")
    if trns is not None:
        if colour in (0, 2):
            raise Refused("a tRNS colour key on a grey or rgb png is not supported;"
                          " export with an alpha channel")
        if colour in (4, 6):
            raise Refused("tRNS chunk on a png that already has alpha")
        if len(trns) > len(palette):
            raise Refused("tRNS chunk longer than the palette")
    if not idat:
        raise Refused("no IDAT chunk")
    try:
        raw = zlib.decompress(b"".join(idat))
    except zlib.error as e:
        raise Refused(f"bad image data ({e})")
    bpp = CHANNELS[colour]
    if len(raw) != h * (1 + w * bpp):
        raise Refused(f"image data is {len(raw)} bytes, expected {h * (1 + w * bpp)}")
    rows = unfilter(raw, w, h, bpp)

    alphas = list(trns or b"") + [255] * (256 - len(trns or b""))
    pixels = []
    for line in rows:
        out = []
        for x in range(w):
            p = line[x * bpp:(x + 1) * bpp]
            if colour == 0:
                out.append((p[0], p[0], p[0], 255))
            elif colour == 2:
                out.append((p[0], p[1], p[2], 255))
            elif colour == 3:
                if p[0] >= len(palette):
                    raise Refused(f"palette index {p[0]} past the {len(palette)}-entry palette")
                r, g, b = palette[p[0]]
                out.append((r, g, b, alphas[p[0]]))
            elif colour == 4:
                out.append((p[0], p[0], p[0], p[1]))
            else:
                out.append((p[0], p[1], p[2], p[3]))
        pixels.append(out)
    return w, h, pixels


def read_sprite(path, name):
    """converts one png; returns a dict describing the sprite, or exits."""
    try:
        with open(path, "rb") as fp:
            data = fp.read()
    except OSError as e:
        sys.exit(f"{path}: {e.strerror}")
    try:
        w, h, pixels = decode_png(data)
    except Refused as e:
        sys.exit(f"{path}: refused: {e}")
    stride = (w + 7) // 8
    ink = bytearray(stride * h)
    mask = bytearray(stride * h)
    pivot = None
    kind = None  # how the pivot pixel was read, for the banner
    painted = 0
    inked = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[y][x]
            if a == 0:
                continue
            # 0.299 r + 0.587 g + 0.114 b >= 128, scaled by 1000 so it is exact
            white = 299 * r + 587 * g + 114 * b >= 128000
            if (r, g, b) in (PIVOT_WHITE, PIVOT_BLACK):
                if pivot is not None:
                    sys.exit(f"{path}: refused: a second pivot pixel at ({x}, {y});"
                             f" the first is at ({pivot[0]}, {pivot[1]})")
                pivot = (x, y)
                if a != 255:
                    kind = f"#{r:02x}{g:02x}{b:02x} pixel at alpha {a}, not painted"
                    continue
                white = (r, g, b) == PIVOT_WHITE
                kind = f"#{r:02x}{g:02x}{b:02x} pixel, painted {'white' if white else 'black'}"
            at = y * stride + x // 8
            bit = 0x80 >> (x & 7)
            mask[at] |= bit
            painted += 1
            if white:
                ink[at] |= bit
                inked += 1
    if pivot is None:
        pivot = (w // 2, h // 2)
    return {"path": path, "src": os.path.basename(path), "name": name, "w": w, "h": h,
            "stride": stride, "pivot": pivot, "kind": kind, "ink": ink,
            "mask": mask, "painted": painted, "inked": inked}


def identifier(stem):
    """a file stem made into a c identifier."""
    s = "".join(ch if ch.isascii() and (ch.isalnum() or ch == "_") else "_" for ch in stem)
    if not s or s[0].isdigit():
        s = "sprite_" + s
    return s


def plane_lines(sym, plane, sp):
    lines = [f"static const uint8_t {sym}[{len(plane)}] SD_PROGMEM = {{"]
    for y in range(sp["h"]):
        row = plane[y * sp["stride"]:(y + 1) * sp["stride"]]
        lines.append("  " + ", ".join(f"0x{v:02x}" for v in row) + ",")
    lines.append("};")
    return lines


def emit(sprites, out):
    guard = identifier(os.path.splitext(os.path.basename(out))[0]).upper() + "_H"
    lines = []
    lines.append("// SPDX-License-Identifier: MIT")
    lines.append("// generated by tools/png2sprite.py - do not edit.")
    lines.append(f"// sprite format {FORMAT}: ink and mask planes in drawBitmap's row layout,"
                 " msb first.")
    for sp in sprites:
        how = f"its {sp['kind']}" if sp["kind"] else "the centre, no pivot pixel"
        lines.append(f"// {sp['name']}: {sp['src']}, {sp['w']}x{sp['h']},"
                     f" pivot ({sp['pivot'][0]}, {sp['pivot'][1]}) from {how}.")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <SD_Sprite.h>")
    lines.append("")
    lines.append(f"static_assert(kSDSpriteFormat == {FORMAT},")
    lines.append(f'              "this header is sprite format {FORMAT};'
                 ' regenerate it with a matching tools/png2sprite.py");')
    for sp in sprites:
        n = sp["name"]
        lines.append("")
        per_row = f"{sp['stride']} byte{'s' if sp['stride'] != 1 else ''} per row"
        lines.append(f"// {n}: {sp['w']}x{sp['h']}, {per_row}")
        lines.extend(plane_lines(f"{n}_ink", sp["ink"], sp))
        lines.append("")
        lines.extend(plane_lines(f"{n}_mask", sp["mask"], sp))
        lines.append("")
        lines.append(f"static const SD_Sprite {n} = {{ {sp['w']}, {sp['h']}, {sp['pivot'][0]},"
                     f" {sp['pivot'][1]}, {n}_ink, {n}_mask }};")
    lines.append("")
    lines.append(f"#endif  // {guard}")
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("png", nargs="+", help="input .png file(s)")
    ap.add_argument("-o", "--out", required=True, help="output .h path")
    ap.add_argument("--name",
                    help="c identifier for the sprite when one png is given"
                         " (default: the file stem)")
    args = ap.parse_args()

    if args.name is not None:
        if len(args.png) != 1:
            sys.exit("--name takes exactly one png; with several, names come from the file stems")
        if not (args.name.isascii() and args.name.isidentifier()):
            sys.exit(f"--name {args.name!r} is not a c identifier")
        names = [args.name]
    else:
        names = [identifier(os.path.splitext(os.path.basename(p))[0]) for p in args.png]
    for i, n in enumerate(names):
        if n in names[:i]:
            sys.exit(f"two sprites would both be named {n}; rename a file")

    sprites = [read_sprite(p, n) for p, n in zip(args.png, names)]
    text = emit(sprites, args.out)
    with open(args.out, "w", encoding="utf-8", newline="\n") as fp:
        fp.write(text)

    for sp in sprites:
        how = sp["kind"] or "centre"
        print(f"{sp['src']}: {sp['w']}x{sp['h']}, pivot ({sp['pivot'][0]}, {sp['pivot'][1]})"
              f" [{how}], {sp['painted']} painted ({sp['inked']} ink,"
              f" {sp['painted'] - sp['inked']} black) -> {sp['name']}")
    print(f"  wrote {args.out}")


if __name__ == "__main__":
    main()
