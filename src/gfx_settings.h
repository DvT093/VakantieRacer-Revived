// Beeldinstellingen van de port: userdata/vracer/graphics.ini [Graphics].
// Gedeeld door de race (main.cpp) en het menu (Opties > Gevorderd), dus
// alleen de standaardbibliotheek.
//
//   [Graphics]
//   Upgrades=1          ; race start upgraded (1) of origineel (0); F9 wisselt
//   Resolution=screen   ; screen | original | <breedte>x<hoogte> (4:3)
//   MSAA=8              ; 1 (uit), 2, 4, 8
//   AlphaToCoverage=1   ; uitgesneden randen (bomen, hekken) ook glad
//   Mipmaps=1           ; mipmaps + trilinear
//   Anisotropy=16       ; 1 (uit) .. 16
//   MenuUpgrades=1      ; menu scherp op schermmaat met HD-plaatjes (1) of
//                       ; origineel 800x600 opgerekt (0); F9 in het menu wisselt
#pragma once

#include <sys/stat.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace vracer {

struct GfxSettings {
  bool upgrades = true;
  // 0x0 = "screen": het venster of scherm, 4:3; -1x-1 = "original": wat
  // het menu kiest (raser.ini RESOLUTION); anders een vaste maat.
  int width = 0, height = 0;
  int msaa = 8;
  bool alpha_to_coverage = true;
  bool mipmaps = true;
  int anisotropy = 16;
  bool menu_upgrades = true;

  bool Screen() const { return width == 0; }
  bool Original() const { return width < 0; }
  std::string ResolutionText() const {
    if (Screen()) return "screen";
    if (Original()) return "original";
    return std::to_string(width) + "x" + std::to_string(height);
  }
  void SetResolution(const std::string& v) {
    int w = 0, h = 0;
    if (v == "original") {
      width = height = -1;
    } else if (std::sscanf(v.c_str(), "%dx%d", &w, &h) == 2 && w >= 320 && h >= 240) {
      width = w;
      height = h;
    } else {
      width = height = 0;  // screen
    }
  }
};

// De speldata-map van de port: $XDG_DATA_HOME/vracer, anders
// ~/Library/Application Support/vracer (zelfde regel als SwARc's DataDir).
inline std::string UserDir() {
  if (const char* x = std::getenv("XDG_DATA_HOME"); x && *x) return std::string(x) + "/vracer";
  const char* home = std::getenv("HOME");
  return std::string(home ? home : ".") + "/Library/Application Support/vracer";
}

// <user dir>/graphics.ini.
inline std::string GfxSettingsPath() { return UserDir() + "/graphics.ini"; }

inline GfxSettings LoadGfxSettings(const std::string& path = GfxSettingsPath()) {
  GfxSettings s;
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return s;
  char line[256];
  while (std::fgets(line, sizeof line, f)) {
    std::string l = line;
    if (size_t c = l.find(';'); c != std::string::npos) l.resize(c);
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    auto trim = [](std::string t) {
      size_t a = t.find_first_not_of(" \t\r\n"), b = t.find_last_not_of(" \t\r\n");
      return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
    };
    std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
    for (char& ch : k) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    int n = std::atoi(v.c_str());
    if (k == "upgrades") s.upgrades = n != 0;
    else if (k == "resolution") s.SetResolution(v);
    else if (k == "msaa") s.msaa = n == 2 || n == 4 || n == 8 ? n : 1;
    else if (k == "alphatocoverage") s.alpha_to_coverage = n != 0;
    else if (k == "mipmaps") s.mipmaps = n != 0;
    else if (k == "anisotropy") s.anisotropy = n < 1 ? 1 : n > 16 ? 16 : n;
    else if (k == "menuupgrades") s.menu_upgrades = n != 0;
  }
  std::fclose(f);
  return s;
}

inline bool SaveGfxSettings(const GfxSettings& s, const std::string& path = GfxSettingsPath()) {
  for (size_t i = 1; i < path.size(); i++)
    if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  std::fprintf(f,
               "; Beeldinstellingen van de Mac-port (Opties > Gevorderd). In de race wisselt\n"
               "; F9 alles tussen origineel en upgraded, F10 alleen anti-aliasing, F11 alleen\n"
               "; de texturefiltering. Resolutie: screen, original of <breedte>x<hoogte>.\n"
               "; MenuUpgrades: het menu scherp met HD-plaatjes (1) of origineel (0); F9 in\n"
               "; het menu wisselt.\n"
               "[Graphics]\n"
               "Upgrades=%d\nResolution=%s\nMSAA=%d\nAlphaToCoverage=%d\nMipmaps=%d\nAnisotropy=%d\n"
               "MenuUpgrades=%d\n",
               s.upgrades ? 1 : 0, s.ResolutionText().c_str(), s.msaa, s.alpha_to_coverage ? 1 : 0,
               s.mipmaps ? 1 : 0, s.anisotropy, s.menu_upgrades ? 1 : 0);
  std::fclose(f);
  return true;
}

}  // namespace vracer
