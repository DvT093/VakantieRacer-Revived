// gm_main.cpp (D:\Work\RED\Game\prog\Gm_main.cpp): the game's main loop.
// Replaced: the pause. In the original, check_game_keys (0x439470) turns
// Esc into "Wilt u echt stoppen!? (J/N)" and P into "Pauze / Druk een
// toets" (0x54CBCA = 2 or 1, overlay state 0x563A2C = 3 or 2), and
// handle_game (0x4390A0) then calls 0x438FA0, which freezes the race:
// it stops the race timers, mutes the sound card through the winmm mixer
// and spins on GetAsyncKeyState until J/Y (quit), N/Esc or any key.
// Here both keys open a pause menu over the frozen frame instead: Doorgaan,
// Opnieuw starten (when a front end runs the race), the image upgrades
// (as F9/F10/F11) and Stoppen (what J did). Drawn with the game's own HUD
// font (overlay.ini [FONT1] on overlay.tga). See re/findings.md "Pauze".

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "game/race_setup.h"
#include "gfx_settings.h"
#include "runtime/audio/mixer.h"
#include "win32/host_link.h"
#include "win32/input_state.h"
#include "x86/present.h"
#include "x86/runtime.h"

namespace recomp::gen {

void f_00439470_lifted(::recomp::Ctx& c);

}  // namespace recomp::gen

namespace {

using recomp::Rd32;
using recomp::Wr32;

constexpr uint32_t kPauseMode = 0x54CBCA;     // 1 P, 2 Esc (handle_game resets it)
constexpr uint32_t kOverlayState = 0x563A2C;  // race overlay: 1 race, 2 pauze, 3 stoppen?
constexpr uint32_t kRenderW = 0x50A9C8, kRenderH = 0x50A9CC;
constexpr uint32_t kFrameTimer = 0x565A88;    // the frame timer (0x4776F0 updates it)
constexpr uint32_t kSetGameState = 0x439280;  // cdecl (state): 4 = end the race
constexpr uint32_t kTimerPause = 0x434C80, kTimerResume = 0x434CF0;  // cdecl (timer)
constexpr uint32_t kOtherTimersPause = 0x41D6E0, kOtherTimersResume = 0x41D740;
constexpr uint32_t kTimerRead = 0x434DC0;     // cdecl (timer): its time in ms
constexpr uint32_t kTimerReset = 0x477830;    // fastcall (timer)
constexpr uint32_t kShowFrame = 0x4916B0;     // flip / blit the back buffer
constexpr int kRaceTimers[] = {0, 3, 0xB};    // the ones 0x438FA0 stops

// ---- the HUD font: overlay.ini [FONT1] glyphs on overlay.tga

struct Glyph {
  int x, y, w, h;
};

struct Font {
  int tw = 0, th = 0;
  std::vector<uint32_t> px;  // ARGB, the key colour (255,0,127) transparent
  std::map<int, Glyph> glyphs;
  bool ok() const { return !px.empty() && !glyphs.empty(); }
};

std::string Lower(std::string s) {
  for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// A file of the install: the overlay's copy, a loose file, else rcs.IMG
// (rcs.ind: u16 count, then {char name[20]; u32 offset}).
std::vector<uint8_t> InstallFile(const std::string& name) {
  for (const std::string& dir : {vracer::UserDir() + "/install", vracer::g_race.game_dir}) {
    std::vector<uint8_t> d = ReadFile(dir + "/" + name);
    if (!d.empty()) return d;
  }
  std::vector<uint8_t> ind = ReadFile(vracer::g_race.game_dir + "/rcs.ind");
  if (ind.size() < 2) return {};
  const int n = ind[0] | ind[1] << 8;
  std::vector<uint32_t> offs;
  uint32_t at = 0;
  bool found = false;
  for (int i = 0; i < n && 2 + 24 * (i + 1) <= int(ind.size()); i++) {
    const uint8_t* e = &ind[2 + 24 * i];
    uint32_t off = e[20] | e[21] << 8 | e[22] << 16 | uint32_t(e[23]) << 24;
    offs.push_back(off);
    if (!found && Lower(std::string(reinterpret_cast<const char*>(e), strnlen(reinterpret_cast<const char*>(e), 20))) == Lower(name)) {
      at = off;
      found = true;
    }
  }
  if (!found) return {};
  std::ifstream img(vracer::g_race.game_dir + "/rcs.IMG", std::ios::binary | std::ios::ate);
  if (!img) return {};
  uint32_t end = uint32_t(img.tellg());
  for (uint32_t o : offs)
    if (o > at && o < end) end = o;
  std::vector<uint8_t> d(end - at);
  img.seekg(at);
  img.read(reinterpret_cast<char*>(d.data()), std::streamsize(d.size()));
  return d;
}

// Uncompressed 24/32-bit TGA.
bool DecodeTga(const std::vector<uint8_t>& d, Font& f) {
  if (d.size() < 18 || d[2] != 2) return false;
  const int w = d[12] | d[13] << 8, h = d[14] | d[15] << 8, bpp = d[16] / 8;
  const bool top = d[17] & 0x20;
  size_t p = 18 + d[0];
  if ((bpp != 3 && bpp != 4) || d.size() < p + size_t(w) * h * bpp) return false;
  f.tw = w;
  f.th = h;
  f.px.resize(size_t(w) * h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++, p += bpp) {
      uint8_t b = d[p], g = d[p + 1], r = d[p + 2];
      bool key = r > 200 && g < 60 && b > 90 && b < 170;
      f.px[size_t(top ? y : h - 1 - y) * w + x] = key ? 0 : 0xFF000000u | r << 16 | g << 8 | b;
    }
  return true;
}

void LoadFontIni(Font& f) {
  std::vector<uint8_t> raw = InstallFile("overlay.ini");
  std::istringstream in(std::string(raw.begin(), raw.end()));
  std::string line, sec;
  static const std::map<std::string, int> kNamed = {{"space", ' '}, {"minus", '-'}, {"div", ':'}, {"slash", '/'}};
  while (std::getline(in, line)) {
    if (auto c = line.find(';'); c != std::string::npos && line.find('=') > c) line.resize(c);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    if (!line.empty() && line[0] == '[') {
      sec = Lower(line.substr(1, line.find(']') - 1));
      continue;
    }
    if (sec != "font1" && sec != "font1_ascii") continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq), val = line.substr(eq + 1);
    key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
    char file[32];
    Glyph g;
    if (key.empty() || std::sscanf(val.c_str(), " %31[^;];%d;%d;%d;%d", file, &g.x, &g.y, &g.w, &g.h) != 5) continue;
    int code;
    if (sec == "font1_ascii") code = std::atoi(key.c_str());
    else if (key.size() == 1) code = std::toupper(static_cast<unsigned char>(key[0]));
    else if (auto it = kNamed.find(Lower(key)); it != kNamed.end()) code = it->second;
    else continue;
    f.glyphs[code] = g;
  }
}

const Font& HudFont() {
  static Font f = [] {
    Font font;
    if (DecodeTga(InstallFile("overlay.tga"), font)) LoadFontIni(font);
    if (!font.ok()) std::fprintf(stderr, "vracer: pauze: geen HUD-font (overlay.tga/overlay.ini)\n");
    return font;
  }();
  return f;
}

// ---- drawing into an ARGB image

struct Image {
  int w, h;
  std::vector<uint32_t> px;
  void Fill(int x0, int y0, int x1, int y1, uint32_t c) {
    for (int y = std::max(0, y0); y < std::min(h, y1); y++)
      for (int x = std::max(0, x0); x < std::min(w, x1); x++) px[size_t(y) * w + x] = c;
  }
};

constexpr int kGlyphGap = 1;  // between letters, in font pixels

int TextWidth(const Font& f, const std::string& s) {
  int w = 0;
  for (char ch : s)
    if (auto it = f.glyphs.find(ch); it != f.glyphs.end()) w += it->second.w + kGlyphGap;
  return w;
}

// `s` (upper case) at (x, y) in font pixels times `k`, white tinted by
// `tint` (the black outline stays black).
void DrawText(Image& img, const Font& f, const std::string& s, int x, int y, float k, uint32_t tint) {
  float cx = float(x);
  const uint32_t tr = tint >> 16 & 255, tg = tint >> 8 & 255, tb = tint & 255;
  for (char ch : s) {
    auto it = f.glyphs.find(ch);
    if (it == f.glyphs.end()) continue;
    const Glyph& g = it->second;
    const int dw = int(std::lround(g.w * k)), dh = int(std::lround(g.h * k));
    for (int dy = 0; dy < dh; dy++)
      for (int dx = 0; dx < dw; dx++) {
        int sx = g.x + std::min(g.w - 1, int(dx / k)), sy = g.y + std::min(g.h - 1, int(dy / k));
        if (sx < 0 || sy < 0 || sx >= f.tw || sy >= f.th) continue;
        uint32_t p = f.px[size_t(sy) * f.tw + sx];
        if (!(p >> 24)) continue;
        int px = int(cx) + dx, py = y + dy;
        if (px < 0 || py < 0 || px >= img.w || py >= img.h) continue;
        uint32_t r = (p >> 16 & 255) * tr / 255, gg = (p >> 8 & 255) * tg / 255, b = (p & 255) * tb / 255;
        img.px[size_t(py) * img.w + px] = 0xFF000000u | r << 16 | gg << 8 | b;
      }
    cx += (g.w + kGlyphGap) * k;
  }
}

// ---- the pause menu

enum Item { kResume, kRestart, kGfxAll, kGfxAa, kGfxFilter, kQuit };

std::vector<Item> Items() {
  std::vector<Item> v = {kResume};
  if (recomp::HostLinked()) v.push_back(kRestart);  // the menu runs it again
  v.insert(v.end(), {kGfxAll, kGfxAa, kGfxFilter, kQuit});
  return v;
}

std::string Label(Item it) {
  using recomp::hle::GfxOn;
  using recomp::hle::GfxPart;
  switch (it) {
    case kResume: return "DOORGAAN";
    case kRestart: return "OPNIEUW STARTEN";
    case kGfxAll: return std::string("BEELD: ") + (GfxOn(GfxPart::All) ? "UPGRADED" : "ORIGINEEL");
    case kGfxAa: return std::string("ANTI-ALIASING: ") + (GfxOn(GfxPart::AntiAliasing) ? "AAN" : "UIT");
    case kGfxFilter: return std::string("FILTERING: ") + (GfxOn(GfxPart::Filtering) ? "AAN" : "UIT");
    case kQuit: return "STOPPEN";
  }
  return "";
}

// The menu in a box in the middle of the frame (frame pixels), as the
// game's HUD: yellow (overlay.ini RGBA 255;255;15) for the chosen line.
void ShowMenu(const std::vector<Item>& items, size_t sel) {
  const Font& f = HudFont();
  const int fw = int(Rd32(kRenderW)), fh = int(Rd32(kRenderH));
  const float k = std::max(1.0f, fh / 480.0f);  // font pixels at 640x480
  const int line = int(24 * k), head = int(40 * k), pad = int(18 * k);
  std::vector<std::string> labels;
  int tw = TextWidth(f, "PAUZE") * 3 / 2;
  for (Item it : items) {
    labels.push_back(Label(it));
    tw = std::max(tw, TextWidth(f, labels.back()));
  }
  Image img{std::min(fw, int(tw * k) + 2 * pad), 0, {}};
  img.h = std::min(fh, pad + head + int(items.size()) * line + pad);
  img.px.assign(size_t(img.w) * img.h, 0xC0000000u);
  const uint32_t border = 0xFFFFFF0Fu;
  const int b = std::max(2, int(k));
  img.Fill(0, 0, img.w, b, border);
  img.Fill(0, img.h - b, img.w, img.h, border);
  img.Fill(0, 0, b, img.h, border);
  img.Fill(img.w - b, 0, img.w, img.h, border);
  const float hk = k * 1.5f;
  DrawText(img, f, "PAUZE", (img.w - int(TextWidth(f, "PAUZE") * hk)) / 2, pad, hk, 0xFFFFFFFFu);
  for (size_t i = 0; i < items.size(); i++) {
    int y = pad + head + int(i) * line;
    if (i == sel) img.Fill(b, y - int(3 * k), img.w - b, y + int(20 * k), 0xC0403000u);
    DrawText(img, f, labels[i], (img.w - int(TextWidth(f, labels[i]) * k)) / 2, y, k,
             i == sel ? 0xFFFFFF0Fu : 0xFFFFFFFFu);
  }
  recomp::hle::SetHostOverlay(img.px.data(), img.w, img.h, (fw - img.w) / 2, (fh - img.h) / 2);
}

// Keys by VK code, from the host's key state (the pump fills it).
bool Down(int vk) { return recomp::g_key_down[vk] != 0; }

enum Result { kGoOn, kEndRace };

// Runs the menu until the player resumes or ends the race. Presents the
// frozen frame (the back buffer the game drew last) with the menu over it.
Result PauseMenu(recomp::Ctx& c) {
  const std::vector<Item> items = Items();
  size_t sel = 0;
  static const int kKeys[] = {0x26, 0x28, 0x25, 0x27, 0x0D, 0x20, 0x1B, 0x50};
  bool prev[256] = {};
  for (int vk : kKeys) prev[vk] = Down(vk);  // the key that opened it is still down
  std::string shown;
  Result result = kGoOn;
  for (bool done = false; !done;) {
    std::string state = std::to_string(sel);
    for (Item it : items) state += "|" + Label(it);
    if (state != shown) {
      ShowMenu(items, sel);
      shown = state;
    }
    recomp::CallGuest(c, kShowFrame, {});  // presents, paces, pumps events
    auto pressed = [&](int vk) { return Down(vk) && !prev[vk]; };
    const bool up = pressed(0x26), down = pressed(0x28), left = pressed(0x25), right = pressed(0x27);
    const bool enter = pressed(0x0D) || pressed(0x20), back = pressed(0x1B) || pressed(0x50);
    for (int vk : kKeys) prev[vk] = Down(vk);
    if (up) sel = (sel + items.size() - 1) % items.size();
    if (down) sel = (sel + 1) % items.size();
    if (back) break;
    if (!enter && !left && !right) continue;
    using recomp::hle::GfxPart;
    switch (items[sel]) {
      case kResume:
        if (enter) done = true;
        break;
      case kRestart:
        if (!enter) break;
        recomp::HostLinkSend("restart");
        result = kEndRace;
        done = true;
        break;
      case kGfxAll: recomp::hle::SwitchGfx(GfxPart::All); break;
      case kGfxAa: recomp::hle::SwitchGfx(GfxPart::AntiAliasing); break;
      case kGfxFilter: recomp::hle::SwitchGfx(GfxPart::Filtering); break;
      case kQuit:
        if (!enter) break;
        result = kEndRace;
        done = true;
        break;
    }
  }
  recomp::hle::SetHostOverlay(nullptr, 0, 0, 0, 0);
  // As the original: let go of the keys first, or check_game_keys would
  // pause again on the next frame.
  while (Down(0x1B) || Down(0x50) || Down(0x0D) || Down(0x20)) recomp::CallGuest(c, kShowFrame, {});
  return result;
}

}  // namespace

namespace recomp::gen {

// 0x439470 check_game_keys(): debug keys, then Esc -> 0x54CBCA = 2 and
// P -> 1 (outside the countdown and the finish, game states 3 and 6),
// each with its overlay state (0x563A2C 3: "Wilt u echt stoppen!? (J/N)",
// 2: "Pauze / Druk een toets"). The pause menu replaces both texts, so
// the overlay keeps its state and the frozen frame shows the race.
void f_00439470(::recomp::Ctx& c) {
  const uint32_t overlay = Rd32(kOverlayState);
  f_00439470_lifted(c);
  if (Rd32(kPauseMode) != 0) Wr32(kOverlayState, overlay);
}

// 0x438FA0 (cdecl, no arguments), from handle_game when 0x54CBCA is set:
// the pause. The original stops the race timers 0, 3 and 11 and the other
// timers (0x41D6E0), mutes the sound card's line 4 (0x49B8C0/0x49B820,
// winmm mixer), waits for its keys (0x438EB0 for P, 0x438F10 for Esc:
// J/Y ends the race with game state 4), then resumes the timers, sets the
// overlay back to 1 and resets the frame timer so the pause leaves no
// jump. Here: the same, with the sound output paused instead of muted and
// the pause menu instead of the key loops.
void f_00438FA0(::recomp::Ctx& c) {
  auto timers = [&] {
    std::string s;
    for (int t : kRaceTimers)
      s += " " + std::to_string(t) + "=" + std::to_string(int32_t(recomp::CallGuest(c, kTimerRead, {uint32_t(t)})));
    return s;
  };
  std::fprintf(stderr, "vracer: pauze, timers%s\n", timers().c_str());
  for (int t : kRaceTimers) recomp::CallGuest(c, kTimerPause, {uint32_t(t)});
  recomp::CallGuest(c, kOtherTimersPause, {});
  recomp::audio::GetMixer().SetPaused(true);
  const Result r = PauseMenu(c);
  std::fprintf(stderr, "vracer: pauze uit (%s)\n", r == kEndRace ? "stoppen" : "doorgaan");
  if (r == kEndRace) recomp::CallGuest(c, kSetGameState, {4});
  Wr32(kOverlayState, 1);
  for (int t : kRaceTimers) recomp::CallGuest(c, kTimerResume, {uint32_t(t)});
  recomp::CallGuest(c, kOtherTimersResume, {});
  recomp::audio::GetMixer().SetPaused(false);
  c.ecx = kFrameTimer;
  recomp::CallGuest(c, kTimerReset, {});
  std::fprintf(stderr, "vracer: verder, timers%s\n", timers().c_str());
  c.esp += 4;  // ret: the original body is not run
}

}  // namespace recomp::gen
