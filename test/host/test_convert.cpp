// SPDX-License-Identifier: MIT
// converter suite: runs tools/png2sprite.py (python3 on the path) on the
// fixture pngs in fixtures/, writing into a fresh temporary directory.
// arm.png (rgba, its rows using all five filters, an opaque #ff00ff pivot on
// the shoulder's outline pixel (0, 3), painted white) converts to a header
// byte-identical to the pinned fixtures/arm_expected.h; the same art as
// indexed with tRNS, grey+alpha, opaque rgb and opaque grey converts to the
// same planes, as each colour type can carry them; the pivot painted black
// (opaque #800080) and left unpainted (either colour at alpha 1..254) leave
// the shoulder pixel black and clear; reading.png pins the frozen colour
// reading pixel by pixel; 16-bit, interlaced, unknown-critical-chunk and
// two-pivot pngs are refused with a message and no output. the pinned header
// itself is compiled in: it validates, its planes are the mockup's arm, and
// drawn at the mockup's angles it is the mockup's frame.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Sprite.h"

#include "fixtures/arm_expected.h"
#include "test_common.h"
#include "test_fixtures.h"

static const char *kTool = "../../tools/png2sprite.py";
static const char *kFixtureDir = "fixtures/";

static std::string readFile(const std::string &path, bool *ok) {
  std::string out;
  FILE *fp = std::fopen(path.c_str(), "rb");
  *ok = fp != nullptr;
  if (fp == nullptr) return out;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
  std::fclose(fp);
  return out;
}

static bool exists(const std::string &path) {
  FILE *fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) return false;
  std::fclose(fp);
  return true;
}

// the converter's text for a 16x7 plane named sym: one line per row.
static std::string plane16x7(const char *sym, const uint8_t *bytes) {
  char line[64];
  std::snprintf(line, sizeof(line), "static const uint8_t %s[14] SD_PROGMEM = {\n", sym);
  std::string out = line;
  for (int r = 0; r < 7; ++r) {
    std::snprintf(line, sizeof(line), "  0x%02x, 0x%02x,\n", bytes[r * 2], bytes[r * 2 + 1]);
    out += line;
  }
  return out + "};";
}

// the text from `start` through the next "};", or empty.
static std::string block(const std::string &text, const std::string &start) {
  const size_t a = text.find(start);
  if (a == std::string::npos) return std::string();
  const size_t b = text.find("};", a);
  if (b == std::string::npos) return std::string();
  return text.substr(a, b + 2 - a);
}

struct Converter {
  std::string dir;
  std::vector<std::string> made;

  Converter() {
    const char *t = std::getenv("TMPDIR");
    std::string base = (t != nullptr && t[0] != 0) ? t : "/tmp";
    if (base.back() != '/') base += '/';
    std::string tmpl = base + "sd_convert_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back(0);
    if (mkdtemp(buf.data()) != nullptr) dir = buf.data();
  }
  ~Converter() {
    for (const std::string &p : made) std::remove(p.c_str());
    if (!dir.empty()) rmdir(dir.c_str());
  }

  // runs the tool on fixture pngs, writing <dir>/<out>; returns the exit
  // status, with the tool's stderr in *err.
  int run(const std::string &pngs, const std::string &out, const std::string &extra,
          std::string *err) {
    const std::string outPath = dir + "/" + out;
    const std::string errPath = dir + "/" + out + ".err";
    made.push_back(outPath);
    made.push_back(errPath);
    std::string cmd = "python3 '" + std::string(kTool) + "'";
    size_t at = 0;
    while (at < pngs.size()) {
      size_t sp = pngs.find(' ', at);
      if (sp == std::string::npos) sp = pngs.size();
      cmd += " '" + std::string(kFixtureDir) + pngs.substr(at, sp - at) + "'";
      at = sp + 1;
    }
    cmd += " -o '" + outPath + "' " + extra + " > /dev/null 2> '" + errPath + "'";
    const int rc = std::system(cmd.c_str());
    bool ok;
    *err = readFile(errPath, &ok);
    return rc;
  }

  std::string output(const std::string &out, bool *ok) { return readFile(dir + "/" + out, ok); }
};

void runConvertTests() {
  Converter cv;
  CHECK(!cv.dir.empty(), "could not make a temporary directory");
  if (cv.dir.empty()) return;

  bool ok = false;
  const std::string pinned = readFile(std::string(kFixtureDir) + "arm_expected.h", &ok);
  CHECK(ok && !pinned.empty(), "fixtures/arm_expected.h missing");
  const std::string pinnedInk = block(pinned, "static const uint8_t arm_ink[");
  const std::string pinnedMask = block(pinned, "static const uint8_t arm_mask[");
  CHECK(!pinnedInk.empty() && !pinnedMask.empty(), "pinned header has no arm planes");

  // ---- arm.png converts to the pinned header, byte for byte ----
  {
    std::string err;
    const int rc = cv.run("arm.png", "arm_expected.h", "--name arm", &err);
    CHECK(rc == 0, "arm.png: converter exited %d: %s", rc, err.c_str());
    const std::string got = cv.output("arm_expected.h", &ok);
    CHECK(ok && got == pinned,
          "arm.png: header differs from fixtures/arm_expected.h (%zu vs %zu bytes)", got.size(),
          pinned.size());
  }

  // ---- the planes, as the converter writes them, are the ascii arm's ----
  CHECK(pinnedInk == plane16x7("arm_ink", kArm.ink) &&
            pinnedMask == plane16x7("arm_mask", kArm.mask),
        "pinned header's planes are not the ascii arm's");

  // ---- the same art in the other colour types: the same planes ----
  {
    struct Variant {
      const char *png;
      const char *pivot;  // the SD_Sprite initializer's leading fields
      bool opaque;        // no alpha to carry: every pixel painted
    };
    static const Variant variants[] = {
        {"arm_indexed.png", "{ 16, 7, 0, 3,", false},  // #ff00ff in the palette
        {"arm_ga.png", "{ 16, 7, 8, 3,", false},       // no pivot colour: the centre
        {"arm_rgb.png", "{ 16, 7, 0, 3,", true},
        {"arm_grey.png", "{ 16, 7, 8, 3,", true},
    };
    uint8_t full[14];
    memset(full, 0xff, sizeof(full));
    const std::string allMasked = plane16x7("arm_mask", full);
    for (const Variant &v : variants) {
      std::string err;
      const std::string out = std::string(v.png) + ".h";
      const int rc = cv.run(v.png, out, "--name arm", &err);
      CHECK(rc == 0, "%s: converter exited %d: %s", v.png, rc, err.c_str());
      const std::string got = cv.output(out, &ok);
      CHECK(block(got, "static const uint8_t arm_ink[") == pinnedInk,
            "%s: ink plane differs from arm.png's", v.png);
      if (v.opaque)
        CHECK(block(got, "static const uint8_t arm_mask[") == allMasked,
              "%s: an opaque png should mask every pixel", v.png);
      else
        CHECK(block(got, "static const uint8_t arm_mask[") == pinnedMask,
              "%s: mask plane differs from arm.png's", v.png);
      CHECK(got.find(std::string("static const SD_Sprite arm = ") + v.pivot) !=
                std::string::npos,
            "%s: wrong size or pivot", v.png);
    }
  }

  // ---- the pivot painted black, and left unpainted in either colour ----
  {
    uint8_t blackInk[14], clearInk[14], clearMask[14];
    memcpy(blackInk, kArm.ink, 14);
    memcpy(clearInk, kArm.ink, 14);
    memcpy(clearMask, kArm.mask, 14);
    blackInk[6] = (uint8_t)(blackInk[6] & ~0x80u);  // pixel (0, 3)
    clearInk[6] = (uint8_t)(clearInk[6] & ~0x80u);
    clearMask[6] = (uint8_t)(clearMask[6] & ~0x80u);
    struct Kind {
      const char *png;
      const uint8_t *ink, *mask;
      const char *banner;
    };
    const Kind kinds[] = {
        {"arm_pivot_black.png", blackInk, kArm.mask,
         "pivot (0, 3) from its #800080 pixel, painted black."},
        {"arm_pivot_unpainted.png", clearInk, clearMask,
         "pivot (0, 3) from its #ff00ff pixel at alpha 254, not painted."},
        {"arm_pivot_unpainted_dark.png", clearInk, clearMask,
         "pivot (0, 3) from its #800080 pixel at alpha 1, not painted."},
    };
    for (const Kind &k : kinds) {
      std::string err;
      const std::string out = std::string(k.png) + ".h";
      const int rc = cv.run(k.png, out, "--name arm", &err);
      CHECK(rc == 0, "%s: converter exited %d: %s", k.png, rc, err.c_str());
      const std::string got = cv.output(out, &ok);
      CHECK(block(got, "static const uint8_t arm_ink[") == plane16x7("arm_ink", k.ink),
            "%s: ink plane wrong at the pivot", k.png);
      CHECK(block(got, "static const uint8_t arm_mask[") == plane16x7("arm_mask", k.mask),
            "%s: mask plane wrong at the pivot", k.png);
      CHECK(got.find("static const SD_Sprite arm = { 16, 7, 0, 3,") != std::string::npos,
            "%s: the pivot should be (0, 3)", k.png);
      CHECK(got.find(k.banner) != std::string::npos, "%s: banner should say \"%s\"", k.png,
            k.banner);
    }
  }

  // ---- the frozen reading, pixel by pixel ----
  // row 0: grey 0, 127, 128, 255, red, green, blue, yellow (opaque)
  //   -> masked all; ink at 128, 255, green, yellow
  // row 1: white at alpha 0, white at alpha 1, black at alpha 1, #ff00ff at
  //   alpha 0, #ff00ff at alpha 255 (the pivot), #ff00fe, #00da00, #00db00
  //   -> masked all but the two at alpha 0; ink at white alpha 1, the pivot,
  //   #00db00
  // row 2: #800080 at alpha 0, then opaque #800081, #810080, #800180,
  //   #ff01ff, #fe00ff (near the pivot colours, none a pivot), white, black
  //   -> masked all but the one at alpha 0; ink at white
  {
    std::string err;
    const int rc = cv.run("reading.png", "reading.h", "--name reading", &err);
    CHECK(rc == 0, "reading.png: converter exited %d: %s", rc, err.c_str());
    const std::string got = cv.output("reading.h", &ok);
    CHECK(block(got, "static const uint8_t reading_ink[") ==
              "static const uint8_t reading_ink[3] SD_PROGMEM = {\n  0x35,\n  0x49,\n  0x02,\n};",
          "reading.png: ink plane is not 0x35 0x49 0x02");
    CHECK(block(got, "static const uint8_t reading_mask[") ==
              "static const uint8_t reading_mask[3] SD_PROGMEM = {\n  0xff,\n  0x6f,\n  0x7f,\n};",
          "reading.png: mask plane is not 0xff 0x6f 0x7f");
    CHECK(got.find("static const SD_Sprite reading = { 8, 3, 4, 1,") != std::string::npos,
          "reading.png: the pivot should be the opaque #ff00ff at (4, 1)");
  }

  // ---- refused, with a message and no output ----
  {
    struct Refusal {
      const char *png;
      const char *says;
    };
    static const Refusal refusals[] = {
        {"arm_16bit.png", "16-bit"},
        {"arm_interlaced.png", "interlaced"},
        {"arm_critical.png", "unknown critical chunk"},
        {"arm_two_pivots.png", "second pivot"},
    };
    for (const Refusal &r : refusals) {
      std::string err;
      const std::string out = std::string(r.png) + ".h";
      const int rc = cv.run(r.png, out, "", &err);
      CHECK(rc != 0, "%s: should be refused, converter exited 0", r.png);
      CHECK(err.find(r.says) != std::string::npos, "%s: message should say \"%s\", said: %s",
            r.png, r.says, err.c_str());
      CHECK(!exists(cv.dir + "/" + out), "%s: a refused conversion wrote a header", r.png);
    }
  }

  // ---- several pngs, one header, each named for its file ----
  {
    std::string err;
    const int rc = cv.run("arm.png mallet.png", "Body.h", "", &err);
    CHECK(rc == 0, "arm.png mallet.png: converter exited %d: %s", rc, err.c_str());
    const std::string got = cv.output("Body.h", &ok);
    CHECK(got.find("#ifndef BODY_H\n#define BODY_H\n") != std::string::npos,
          "Body.h: include guard should be BODY_H");
    CHECK(got.find("static const SD_Sprite arm = { 16, 7, 0, 3, arm_ink, arm_mask };") !=
              std::string::npos,
          "Body.h: arm sprite missing");
    CHECK(got.find("static const SD_Sprite mallet = { 12, 5, 6, 2, mallet_ink, mallet_mask };") !=
              std::string::npos,
          "Body.h: mallet sprite missing (pivot should be the centre, (6, 2))");
    CHECK(block(got, "static const uint8_t arm_ink[") == pinnedInk,
          "Body.h: arm planes differ from the single-png header");
    const int rc2 = cv.run("arm.png mallet.png", "Named.h", "--name both", &err);
    CHECK(rc2 != 0, "--name with two pngs should be refused");
  }

  // ---- the pinned header, compiled in ----
  {
    CHECK(sd_validateSprite(arm), "arm_expected.h: sprite fails sd_validateSprite");
    CHECK(arm.w == kArm.sprite.w && arm.h == kArm.sprite.h, "arm_expected.h: size %dx%d",
          arm.w, arm.h);
    CHECK(arm.pivotX == 0 && arm.pivotY == 3, "arm_expected.h: pivot (%d, %d), want (0, 3)",
          arm.pivotX, arm.pivotY);
    const size_t n = (size_t)((arm.w + 7) / 8) * (size_t)arm.h;
    CHECK(sizeof(arm_ink) == n && sizeof(arm_mask) == n, "arm_expected.h: plane sizes");
    CHECK(memcmp(arm.ink, kArm.sprite.ink, n) == 0 && memcmp(arm.mask, kArm.sprite.mask, n) == 0,
          "arm_expected.h: planes differ from the mockup's ascii arm");

    // drawn at the mockup's angles, the converted arm is the mockup's arm,
    // and with the mallet the mockup's frame
    OwnedCanvas fromPng(kMockupW, kMockupH), fromArt(kMockupW, kMockupH);
    fromPng.canvas.clear(false);
    fromArt.canvas.clear(false);
    drawMockup(fromPng.canvas, arm, nullptr);
    drawMockup(fromArt.canvas, kArm.sprite, nullptr);
    writeFrame(fromPng.canvas, "convert_arm_mockup");
    CHECK(memcmp(fromPng.canvas.buffer(), fromArt.canvas.buffer(), fromPng.canvas.bytes()) == 0,
          "converted arm at the mockup's angles differs from the ascii arm");
    fromPng.canvas.clear(false);
    drawMockup(fromPng.canvas, arm, &kMallet.sprite);
    const uint64_t h = canvasHash(fromPng.canvas, "convert_mockup");
    CHECK(h == GOLDEN_MOCKUP, "converted arm mockup: hash 0x%016llxULL != pinned 0x%016llxULL",
          (unsigned long long)h, (unsigned long long)GOLDEN_MOCKUP);
  }
}
