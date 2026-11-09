// vracer: Vakantie Racer (Davilex, 2000), natively.
//
//   vracer [--game-dir DIR] [--mods DIR] [-- GAME ARGS...]
//
// DIR is the game's data folder from the user's own CD (the folder with
// spel.dat, rcs.IMG, game.ini). Defaults to $VRACER_GAME_DIR.
// --mods is the race mod folder (default: mods/race of the project this
// binary was built in, or $VRACER_MODS); see src/red/race_mods.h.
// The disc itself (the game checks for it) is the folder above DIR, the
// root of the extracted CD; override with --cd-dir.
// Set VRACER_TRACE=1 to log every HLE call.

#include <sys/stat.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "game/race_setup.h"
#include "gfx_settings.h"
#include "red/race_mods.h"
#include "runtime/log.h"
#include "win32/process.h"

namespace vracer {
extern int g_render_w, g_render_h;  // gfx_resolution.cpp
void ScreenSize(int* w, int* h);
}

int main(int argc, char** argv) {
  std::string game_dir, cd_dir, game_args, mods_dir;
  if (const char* e = std::getenv("VRACER_GAME_DIR")) game_dir = e;
  if (const char* e = std::getenv("VRACER_MODS")) mods_dir = e;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--game-dir") && i + 1 < argc) {
      game_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--cd-dir") && i + 1 < argc) {
      cd_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--mods") && i + 1 < argc) {
      mods_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--")) {
      for (i++; i < argc; i++) {
        if (!game_args.empty()) game_args += ' ';
        game_args += argv[i];
      }
    } else {
      std::fprintf(stderr, "usage: %s [--game-dir DIR] [--mods DIR] [-- GAME ARGS...]\n", argv[0]);
      return 2;
    }
  }
  if (game_dir.empty()) {
    std::fprintf(stderr, "vracer: pass --game-dir or set VRACER_GAME_DIR\n");
    return 2;
  }
  while (game_dir.size() > 1 && game_dir.back() == '/') game_dir.pop_back();
  if (const char* t = std::getenv("VRACER_TRACE")) recomp::g_trace = *t && *t != '0';

  std::string exe = game_dir + "/spel.dat";
  struct stat st;
  if (stat(exe.c_str(), &st) != 0) {
    std::fprintf(stderr, "vracer: %s not found\n", exe.c_str());
    return 2;
  }

  // Race mods (mods/race): ini keys, textures and the vehicle swap into
  // SwARc's overlay, before the game reads anything.
  if (mods_dir.empty()) {
    char self[PATH_MAX];
    if (realpath(argv[0], self)) {
      std::string root = self;  // <project>/out/build/release/vracer
      for (int up = 0; up < 4; up++) root = root.substr(0, root.find_last_of('/'));
      if (stat((root + "/mods/race").c_str(), &st) == 0) mods_dir = root + "/mods/race";
    }
  }
  {
    red::RaceModConfig rm;
    rm.game_dir = game_dir;
    rm.overlay_dir = vracer::UserDir() + "/install";
    if (!mods_dir.empty()) rm.mod_dirs = {mods_dir, mods_dir + "/local"};
    rm.port_files = {"vracer.ini"};
    rm.record_files = {{"cars%d.dat", 0x7C}, {"Phys%d.dat", 0xF0}};
    vracer::g_race.player = red::ApplyRaceMods(rm);
    if (!mods_dir.empty()) vracer::LoadRaceSettings(mods_dir + "/vracer.ini");
    const red::PlayerSetup& p = vracer::g_race.player;
    std::fprintf(stderr, "vracer: car %d, vehicle %d, trailer %s (mods %s)\n", p.car, p.vehicle,
                 p.trailer_set ? p.trailer.c_str() : "standard", mods_dir.empty() ? "-" : mods_dir.c_str());
  }

  vracer::g_race.game_dir = game_dir;
  recomp::win32::GameConfig game;
  game.id = "vracer";
  game.guest_dir = "C:\\Vakantie Racer\\data";
  game.exe_name = "spel.dat";
  // The original disc (volume label VRACER) in drive D:.
  if (cd_dir.empty()) cd_dir = game_dir.substr(0, game_dir.find_last_of('/'));
  game.cd_drives = {{'D', cd_dir, "VRACER"}};
  // The game has no frame limiter and the player's car speeds up per
  // frame: 60 fps as on the hardware of its day. VRACER_MAX_FPS=0 uncaps.
  game.max_fps = 60;

  // Beeld-upgrades uit graphics.ini (Opties > Gevorderd): rendermaat,
  // MSAA met alpha-to-coverage, mipmaps met trilinear en anisotropic
  // filtering. F9 wisselt in de race alles tussen origineel en upgraded,
  // F10 alleen de anti-aliasing, F11 alleen de filtering.
  // VRACER_RESOLUTION=screen|original|WxH overschrijft de maat.
  vracer::GfxSettings gs = vracer::LoadGfxSettings();
  if (const char* r = std::getenv("VRACER_RESOLUTION")) gs.SetResolution(r);
  vracer::g_render_w = gs.width;
  vracer::g_render_h = gs.height;
  if (gs.Screen()) vracer::ScreenSize(&vracer::g_render_w, &vracer::g_render_h);
  game.gfx.msaa = gs.msaa;
  game.gfx.alpha_to_coverage = gs.alpha_to_coverage;
  game.gfx.mipmaps = gs.mipmaps;
  game.gfx.anisotropy = gs.anisotropy;
  game.gfx_enhanced = gs.upgrades;
  // Turbo op Cmd: macOS houdt Ctrl+pijltje voor zichzelf (Spaces), dus
  // de race ziet de Command-toetsen als Ctrl (game.ini EXTRA1=17) en de
  // echte Ctrl-toetsen niet.
  game.key_aliases = {{SDL_SCANCODE_LGUI, SDL_SCANCODE_LCTRL},
                      {SDL_SCANCODE_RGUI, SDL_SCANCODE_RCTRL},
                      {SDL_SCANCODE_LCTRL, 0},
                      {SDL_SCANCODE_RCTRL, 0}};
  game.gfx_key_all = SDL_SCANCODE_F9;
  game.gfx_key_aa = SDL_SCANCODE_F10;
  game.gfx_key_filter = SDL_SCANCODE_F11;
  game.gfx_label_original = "Origineel";
  game.gfx_label_enhanced = "Upgraded";
  // HD-textures (tools/make_texture_pack.py, uit je eigen CD): SwARc
  // herkent elke texture aan zijn pixels en tekent de 4x-versie zolang
  // Upgraded aan staat (F9). VRACER_TEX_PACK=map overschrijft de plek.
  game.gfx.replace_textures = true;
  {
    char self[PATH_MAX];
    if (realpath(argv[0], self)) {
      std::string root = self;  // <project>/out/build/release/vracer
      for (int up = 0; up < 4; up++) root = root.substr(0, root.find_last_of('/'));
      if (stat((root + "/build/texture_pack").c_str(), &st) == 0) game.texture_pack = root + "/build/texture_pack";
    }
  }
  return recomp::RunProcess(game, exe, game_dir, game_args);
}
