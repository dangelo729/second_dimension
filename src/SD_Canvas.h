// SPDX-License-Identifier: MIT
#ifndef SD_CANVAS_H
#define SD_CANVAS_H

#include <stddef.h>
#include <stdint.h>

#include "SD_Config.h"
#include "SD_Target.h"

// a 1-bit framebuffer target over a caller-owned buffer, in drawBitmap's row
// layout (the sprite planes' layout): (w + 7) / 8 bytes per row, msb first,
// 1 = lit. the buffer must hold at least bytesFor(w, h) bytes; the canvas
// never allocates. on a board it is an off-screen buffer a sketch can hand to
// drawBitmap; in the host suite it is the canvas every frame is hashed from.
class SD_Canvas : public SD_Target {
 public:
  // a negative size is taken as zero, and so is any size over a null buffer:
  // such a canvas plots nothing and reads dark.
  SD_Canvas(uint8_t *buffer, int16_t w, int16_t h);

  // the bytes a w x h canvas's buffer needs, a constant expression, so it
  // can size an array: uint8_t buf[SD_Canvas::bytesFor(128, 64)].
  static constexpr size_t bytesFor(int16_t w, int16_t h) {
    return w > 0 && h > 0 ? (size_t)((w + 7) / 8) * (size_t)h : 0;
  }

  int16_t width() const override { return w_; }
  int16_t height() const override { return h_; }

  // sets or clears one pixel; a coordinate outside the canvas is ignored.
  void plot(int16_t x, int16_t y, bool on) override;

  // fills the whole buffer, padding bits included: dark by default.
  void clear(bool on = false);

  // reads one pixel back; outside the canvas reads dark.
  bool pixel(int16_t x, int16_t y) const;

  int16_t stride() const { return (int16_t)((w_ + 7) / 8); }
  uint8_t *buffer() { return buf_; }
  const uint8_t *buffer() const { return buf_; }
  size_t bytes() const { return (size_t)stride() * (size_t)h_; }

 private:
  uint8_t *buf_;
  int16_t w_, h_;
};

#endif  // SD_CANVAS_H
