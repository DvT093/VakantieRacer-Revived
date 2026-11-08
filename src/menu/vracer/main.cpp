// vracer-menu: native replacement for Vakantie Racer's Delphi menu
// (MenuData/Menu.exe). Layouts, pictures, fonts and texts are read at run
// time from the user's own CD; races run through the native vracer binary.
//
//   vracer-menu --game-dir DIR [--mods DIR] [--race BIN] [--script FILE]
//               [--shot FILE] [--no-intro]
//
// DIR is the CD's data folder (spel.dat, MenuData/). --mods is the menu mod
// folder (default: mods/menu next to the project), --race the race binary
// (default: vracer next to this binary). --script runs scripted input for
// tests (see App::LoadScript); F12 saves a screenshot to --shot.

#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "engine/app.h"
#include "vracer/game.h"
#include "vracer/screens.h"

int main(int argc, char** argv) {
  std::string game_dir, mods_dir, race_bin, script, shot;
  bool intro = true;
  if (const char* e = std::getenv("VRACER_GAME_DIR")) game_dir = e;
  for (int i = 1; i < argc; i++) {
    auto next = [&](std::string* out) {
      if (i + 1 < argc) *out = argv[++i];
    };
    if (!std::strcmp(argv[i], "--game-dir")) next(&game_dir);
    else if (!std::strcmp(argv[i], "--mods")) next(&mods_dir);
    else if (!std::strcmp(argv[i], "--race")) next(&race_bin);
    else if (!std::strcmp(argv[i], "--script")) next(&script);
    else if (!std::strcmp(argv[i], "--shot")) next(&shot);
    else if (!std::strcmp(argv[i], "--no-intro")) intro = false;
    else {
      std::fprintf(stderr, "usage: %s --game-dir DIR [--mods DIR] [--race BIN] [--script FILE] [--shot FILE] [--no-intro]\n",
                   argv[0]);
      return 2;
    }
  }
  if (game_dir.empty()) {
    std::fprintf(stderr, "vracer-menu: pass --game-dir or set VRACER_GAME_DIR\n");
    return 2;
  }
  while (game_dir.size() > 1 && game_dir.back() == '/') game_dir.pop_back();
  char self[PATH_MAX];
  std::string bindir = realpath(argv[0], self) ? std::string(self) : std::string(argv[0]);
  bindir = bindir.substr(0, bindir.find_last_of('/'));
  if (race_bin.empty()) race_bin = bindir + "/vracer";

  davi::DelphiExe exe;
  std::string menu_exe = davi::FindNoCase(game_dir, "MenuData/Menu.exe");
  if (menu_exe.empty() || !exe.Load(menu_exe)) {
    std::fprintf(stderr, "vracer-menu: cannot read %s/MenuData/Menu.exe\n", game_dir.c_str());
    return 1;
  }
  davi::Assets assets;
  if (!mods_dir.empty()) assets.AddDir(mods_dir);
  assets.AddDir(davi::FindNoCase(game_dir, "MenuData"));

  davi::Skin skin;
  skin.exe = &exe;
  skin.assets = &assets;
  skin.caps.AddLayer(davi::FindNoCase(game_dir, "MenuData/MenuCaps.ini"));
  if (!mods_dir.empty()) skin.caps.AddLayer(mods_dir + "/MenuCaps.ini");
  // Fonts embedded in the data module (MenuCaps "Font=1" Impact, "Font=2" Arial).
  auto data = exe.Form("TData");
  for (int i = 1; i <= 9 && data; i++) {
    davi::Component* f = data->Find("MenuFont" + std::to_string(i));
    if (!f) continue;
    auto font = std::make_unique<davi::Font>();
    std::string over = mods_dir.empty() ? "" : davi::FindNoCase(mods_dir, "MenuFont" + std::to_string(i) + ".ttf");
    bool ok = over.empty() ? font->Load(davi::SizedBytes(*f->Prop("Data"))) : font->LoadFile(over);
    if (ok) skin.fonts[i] = std::move(font);
  }
  vr::RegisterSkin(skin);

  davi::App app(skin);
  davi::AppConfig cfg;
  cfg.title = "Vakantie Racer";
  cfg.screenshot_path = shot;
  if (const char* fs = std::getenv("VRACER_FULLSCREEN")) cfg.fullscreen = *fs && std::strcmp(fs, "window") && *fs != '0';
  if (!app.Init(cfg)) {
    std::fprintf(stderr, "vracer-menu: cannot open the window: %s\n", SDL_GetError());
    return 1;
  }

  red::Install install{game_dir, red::Install::DefaultUserDir("vracer")};
  vr::Game game(app, install, {race_bin, "--game-dir", game_dir});
  if (!game.Load()) {
    std::fprintf(stderr, "vracer-menu: static.ini has no characters or tracks\n");
    return 1;
  }
  // Sounds of the data module: wavBtnDown on every button press
  // (TData.btnDefButtonStateChange), wavRequest/wavReqClose for dialogs.
  for (const char* n : {"wavBtnDown", "wavRequest", "wavReqClose"})
    if (davi::Component* w = data ? data->Find(n) : nullptr)
      game.embedded[n] = davi::LoadWav(davi::SizedBytes(*w->Prop("WavFileData")));
  app.on_button_down = [&](davi::Widget&) { game.Sound("__wavBtnDown"); };
  if (!script.empty()) app.LoadScript(script);
  vr::Start(game, intro);
  return app.Run();
}
