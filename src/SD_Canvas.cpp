// SPDX-License-Identifier: MIT

#include "SD_Canvas.h"

#include <string.h>

SD_Canvas::SD_Canvas(uint8_t *buffer, int16_t w, int16_t h)
    : buf_(buffer), w_(w > 0 ? w : 0), h_(h > 0 ? h : 0) {}

void SD_Canvas::plot(int16_t x, int16_t y, bool on) {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
  uint8_t &cell = buf_[(int32_t)y * stride() + (x >> 3)];
  const uint8_t bit = (uint8_t)(0x80u >> (x & 7));
  if (on)
    cell = (uint8_t)(cell | bit);
  else
    cell = (uint8_t)(cell & ~bit);
}

void SD_Canvas::clear(bool on) { memset(buf_, on ? 0xff : 0x00, bytes()); }

bool SD_Canvas::pixel(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
  return (buf_[(int32_t)y * stride() + (x >> 3)] & (0x80u >> (x & 7))) != 0;
}
