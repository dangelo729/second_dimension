// SPDX-License-Identifier: MIT
// host test runner: owns the shared infrastructure and executes the suites.
// build and run via the Makefile in this directory (`make run`), which runs
// it from this directory: the converter suite finds its fixtures and the
// tool by relative path.

#include <cstdlib>

#include "test_common.h"

long checks = 0;
int failures = 0;
uint32_t lcg = 0x12345678u;

uint64_t fnv1a(const uint8_t *p, size_t n, uint64_t h) {
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}

void writeFrame(const SD_Canvas &canvas, const char *label) {
  const char *dir = std::getenv("SD_FRAMES_DIR");
  if (dir == nullptr || dir[0] == 0) return;
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/%s.pbm", dir, label);
  FILE *fp = std::fopen(path, "wb");
  if (fp == nullptr) {
    std::printf("note: could not write %s\n", path);
    return;
  }
  // pbm's 1 is black, the canvas's 1 is lit: invert so lit shows white.
  std::fprintf(fp, "P4\n%d %d\n", (int)canvas.width(), (int)canvas.height());
  const uint8_t *b = canvas.buffer();
  for (size_t i = 0; i < canvas.bytes(); ++i) std::fputc((uint8_t)~b[i], fp);
  std::fclose(fp);
}

uint64_t canvasHash(const SD_Canvas &canvas, const char *label) {
  writeFrame(canvas, label);
  return fnv1a(canvas.buffer(), canvas.bytes());
}

float frand(float lo, float hi) {
  lcg = lcg * 1664525u + 1013904223u;
  return lo + (hi - lo) * ((lcg >> 8) & 0xFFFFFF) / 16777216.0f;
}

static void runSuite(const char *name, void (*fn)()) {
  const long c0 = checks;
  const int f0 = failures;
  fn();
  std::printf("%-8s %6ld checks, %d failed\n", name, checks - c0, failures - f0);
}

int main() {
  runSuite("sprite", runSpriteTests);
  runSuite("xform", runXformTests);
  runSuite("draw", runDrawTests);
  runSuite("rig", runRigTests);
  runSuite("ik", runIkTests);
  runSuite("group", runGroupTests);
  runSuite("convert", runConvertTests);

  if (failures == 0)
    std::printf("\nALL %ld CHECKS PASSED\n", checks);
  else
    std::printf("\n%d OF %ld CHECK(S) FAILED\n", failures, checks);
  return failures ? 1 : 0;
}
