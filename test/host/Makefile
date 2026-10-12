# SPDX-License-Identifier: MIT
#
# host test harness for SecondDimension: pure clang++, no arduino, no
# hardware. `make` builds sd_host_tests; `make run` builds and executes it
# from this directory.
#
# -ffp-contract=off is load-bearing: the bit-exact golden pins (frame hashes
# of the fixture sprites at pinned angles) depend on the exact floating-point
# expression shapes in SD_Xform and sd_draw, and fma contraction changes
# last-ulp results and produces false golden failures.
#
# the converter suite runs tools/png2sprite.py through python3 from inside
# the binary, on the pngs in fixtures/, so python 3.9 or later must be on
# the path. SD_FRAMES_DIR=<dir> makes the suite write every hashed frame
# there as a pbm, to look at before pinning.
#
# the suite is the library's acceptance gate.

CXX ?= clang++
CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -ffp-contract=off
CPPFLAGS := -I../../src

CORE_SRC := $(wildcard ../../src/*.cpp)
TEST_SRC := $(wildcard test_*.cpp)
HDRS := $(wildcard ../../src/*.h) test_common.h test_fixtures.h fixtures/arm_expected.h

sd_host_tests: $(CORE_SRC) $(TEST_SRC) $(HDRS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(CORE_SRC) $(TEST_SRC) -o $@

.PHONY: run clean

run: sd_host_tests
	./sd_host_tests

clean:
	rm -rf sd_host_tests sd_host_tests.dSYM
