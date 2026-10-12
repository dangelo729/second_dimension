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
// two-pivot pngs are refused with a message and no output. --silhouette:
// outlined.png (an opaque rgb drawing like daedalus's head, a closed 1 px
// outline on black touching all four edges, a face inside) converts to the
// filled shape with only the face in ink, and drawn alone as a group gives
// back the plain conversion's pixels; outlined_gap.png (one outline pixel
// black) is refused; arm.png (alpha outside) converts to the pinned planes;
// a png not among those given is refused. the pinned header itself is
// compiled in: it validates, its planes are the mockup's arm, and drawn at
// the mockup's angles it is the mockup's frame.

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "SD_Canvas.h"
#include "SD_Draw.h"
#include "SD_Group.h"
#include "SD_Rig.h"
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

// a sprite read back from a header the converter wrote: its planes as
// exact-size heap copies (so asan sees any read past one) and its view.
struct Parsed {
  std::vector<uint8_t> ink, mask;
  int w, h, px, py;
  bool ok;
  SD_Sprite view() const {
    return {(int16_t)w, (int16_t)h, (int16_t)px, (int16_t)py, ok ? ink.data() : nullptr,
            ok ? mask.data() : nullptr};
  }
};

static std::vector<uint8_t> planeBytes(const std::string &text, const std::string &sym) {
  std::vector<uint8_t> out;
  const std::string b = block(text, "static const uint8_t " + sym + "[");
  for (size_t at = b.find("0x"); at != std::string::npos; at = b.find("0x", at + 2))
    out.push_back((uint8_t)std::strtoul(b.c_str() + at, nullptr, 16));
  return out;
}

static Parsed parsed(const std::string &text, const std::string &name) {
  Parsed p;
  p.ink = planeBytes(text, name + "_ink");
  p.mask = planeBytes(text, name + "_mask");
  p.w = p.h = p.px = p.py = 0;
  const size_t at = text.find("static const SD_Sprite " + name + " = {");
  p.ok = at != std::string::npos &&
         std::sscanf(text.c_str() + text.find('{', at), "{ %d, %d, %d, %d,", &p.w, &p.h, &p.px,
                     &p.py) == 4 &&
         p.w > 0 && p.h > 0 && p.ink.size() == (size_t)((p.w + 7) / 8 * p.h) &&
         p.mask.size() == p.ink.size();
  return p;
}

// the --silhouette fixture as drawn ('#' white, '-' black, opaque), and as
// the option reads it: the background unpainted, the outline black, the
// face the only ink.
const ArtFixture kOutlined("outlined",
                           {
                               "-----######-----",
                               "---##------##---",
                               "--#----------#--",
                               "-#---##--##---#-",
                               "-#------------#-",
                               "#------#-------#",
                               "-#-----#------#-",
                               "-#---#####----#-",
                               "--##--------##--",
                               "----########----",
                           },
                           8, 5);

const ArtFixture kOutlinedSolid("outlined_solid",
                                {
                                    ".....------.....",
                                    "...----------...",
                                    "..------------..",
                                    ".----##--##----.",
                                    ".--------------.",
                                    "-------#--------",
                                    ".------#-------.",
                                    ".----#####-----.",
                                    "..------------..",
                                    "....--------....",
                                },
                                8, 5);

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

  // ---- --silhouette: an outline on black becomes a group piece ----
  {
    std::string err;
    const int rc = cv.run("outlined.png", "outlined.h", "", &err);
    CHECK(rc == 0, "outlined.png: converter exited %d: %s", rc, err.c_str());
    const Parsed plain = parsed(cv.output("outlined.h", &ok), "outlined");
    CHECK(plain.ok && plain.ink.size() == 20 && memcmp(plain.ink.data(), kOutlined.ink, 20) == 0 &&
              memcmp(plain.mask.data(), kOutlined.mask, 20) == 0,
          "outlined.png: read plainly, it is not the drawing the suite describes");

    const int rc2 = cv.run("outlined.png", "outlined_solid.h",
                           "--silhouette '" + std::string(kFixtureDir) + "outlined.png'", &err);
    CHECK(rc2 == 0, "outlined.png --silhouette: converter exited %d: %s", rc2, err.c_str());
    const std::string text = cv.output("outlined_solid.h", &ok);
    const Parsed solid = parsed(text, "outlined");
    CHECK(solid.ok && solid.mask.size() == 20 &&
              memcmp(solid.mask.data(), kOutlinedSolid.mask, 20) == 0,
          "outlined.png --silhouette: the mask is not the filled shape");
    CHECK(solid.ok && solid.ink.size() == 20 &&
              memcmp(solid.ink.data(), kOutlinedSolid.ink, 20) == 0,
          "outlined.png --silhouette: the ink is not the face alone");
    CHECK(solid.ok && solid.w == 16 && solid.h == 10 && solid.px == 8 && solid.py == 5,
          "outlined.png --silhouette: size or pivot moved");
    CHECK(text.find("//   --silhouette: 40 px of background unpainted, 34 px of outline painted "
                    "black.") != std::string::npos,
          "outlined.png --silhouette: the banner does not name the option");

    // drawn alone as a group, the solid piece gives back the outline: the
    // same canvas as the plain conversion drawn by sd_draw
    const SD_Sprite plainView = plain.view(), solidView = solid.view();
    for (int qi = 0; qi < 2; ++qi) {
      const SD_Sample q = qi == 0 ? SD_SAMPLE_1 : SD_SAMPLE_2x2;
      OwnedCanvas want(32, 20), got(32, 20), sil(32, 20), ink(32, 20);
      want.canvas.clear(false);
      got.canvas.clear(false);
      sd_draw(want.canvas, plainView, SD_Xform::rigid(12.0f, 9.0f, 0.0f), q);
      SD_Bone bone[1] = {{-1, 12.0f, 9.0f, 0.0f, false, false, &solidView}};
      SD_Xform world[1];
      SD_Rig rig(bone, world, 1);
      const int list[1] = {0};
      CHECK(rig.pose() && sd_drawGroup(got.canvas, rig, bone, list, 1, sil.canvas, ink.canvas, q),
            "outlined.png --silhouette: the group did not draw");
      writeFrame(got.canvas, qi == 0 ? "convert_silhouette_1" : "convert_silhouette_2x2");
      CHECK(memcmp(want.canvas.buffer(), got.canvas.buffer(), want.canvas.bytes()) == 0,
            "outlined.png --silhouette, drawn as a group (sample %d): not the outline drawn", qi);
    }
  }

  // ---- --silhouette: a gap in the outline is refused ----
  {
    std::string err;
    const int rc = cv.run("outlined_gap.png", "outlined_gap.h",
                          "--silhouette '" + std::string(kFixtureDir) + "outlined_gap.png'", &err);
    CHECK(rc != 0, "outlined_gap.png --silhouette: should be refused, converter exited 0");
    CHECK(err.find("nothing inside an outline") != std::string::npos,
          "outlined_gap.png --silhouette: message should say \"nothing inside an outline\", said: %s",
          err.c_str());
    CHECK(!exists(cv.dir + "/outlined_gap.h"), "outlined_gap.png: a refused conversion wrote a header");
  }

  // ---- --silhouette: a drawing with alpha outside is read as is ----
  {
    std::string err;
    const int rc = cv.run("arm.png", "arm_silhouette.h",
                          "--name arm --silhouette '" + std::string(kFixtureDir) + "arm.png'", &err);
    CHECK(rc == 0, "arm.png --silhouette: converter exited %d: %s", rc, err.c_str());
    const std::string got = cv.output("arm_silhouette.h", &ok);
    CHECK(block(got, "static const uint8_t arm_ink[") == pinnedInk &&
              block(got, "static const uint8_t arm_mask[") == pinnedMask &&
              got.find("static const SD_Sprite arm = { 16, 7, 0, 3, arm_ink, arm_mask };") !=
                  std::string::npos,
          "arm.png --silhouette: the planes or the sprite differ from the pinned header's");
    CHECK(got.find("--silhouette: its background transparent already, read as is.") !=
              std::string::npos,
          "arm.png --silhouette: the banner should say it was read as is");
    const int rc2 = cv.run("arm.png", "arm_other.h", "--silhouette 'fixtures/mallet.png'", &err);
    CHECK(rc2 != 0 && err.find("not one of the pngs given") != std::string::npos,
          "--silhouette naming a png not given should be refused, said: %s", err.c_str());
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
