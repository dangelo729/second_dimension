// SPDX-License-Identifier: MIT
#ifndef SD_TEST_COMMON_H
#define SD_TEST_COMMON_H

// shared infrastructure for the host test suites: the check macro and its
// counters, the fnv-1a hash every golden pins, and the frame writer.
// definitions live in test_main.cpp.

#include <stddef.h>
#include <stdint.h>

#include <cstdio>

#include "SD_Canvas.h"

// every CHECK evaluated, and every one that failed, across all suites; the
// process exit code keys off failures.
extern long checks;
extern int failures;

// non-fatal assertion: records the failure and keeps going so a single run
// reports every broken invariant.
#define CHECK(cond, ...)                                \
  do {                                                  \
    ++checks;                                           \
    if (!(cond)) {                                      \
      ++failures;                                       \
      std::printf("FAIL %s:%d: ", __FILE__, __LINE__);  \
      std::printf(__VA_ARGS__);                         \
      std::printf("\n");                                \
    }                                                   \
  } while (0)

// fnv-1a over raw bytes, continuing from h (the offset basis by default) so
// several buffers can chain into one hash.
static const uint64_t kFnvBasis = 1469598103934665603ULL;
uint64_t fnv1a(const uint8_t *p, size_t n, uint64_t h = kFnvBasis);

// fnv-1a over a canvas's whole buffer, padding bits included: the frame hash
// the goldens pin. when SD_FRAMES_DIR is set, the canvas is also written
// there as <label>.pbm, so a frame can be looked at before it is pinned.
uint64_t canvasHash(const SD_Canvas &canvas, const char *label);

// writes a canvas as a binary pbm (lit pixels white) into SD_FRAMES_DIR,
// when it is set.
void writeFrame(const SD_Canvas &canvas, const char *label);

// deterministic lcg so fuzz runs are reproducible.
extern uint32_t lcg;
float frand(float lo, float hi);

// suite entry points, run in this order by test_main.cpp.
void runSpriteTests();
void runXformTests();
void runDrawTests();
void runRigTests();
void runIkTests();
void runConvertTests();

#endif  // SD_TEST_COMMON_H
