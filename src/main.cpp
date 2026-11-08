// vracer: Vakantie Racer (Davilex, 2000), natively.
//
//   vracer [--game-dir DIR] [-- GAME ARGS...]
//
// DIR is the game's data folder from the user's own CD (the folder with
// spel.dat, rcs.IMG, game.ini). Defaults to $VRACER_GAME_DIR.
// The disc itself (the game checks for it) is the folder above DIR, the
// root of the extracted CD; override with --cd-dir.
// Set VRACER_TRACE=1 to log every HLE call.

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "runtime/log.h"
#include "win32/process.h"

int main(int argc, char** argv) {
  std::string game_dir, cd_dir, game_args;
  if (const char* e = std::getenv("VRACER_GAME_DIR")) game_dir = e;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--game-dir") && i + 1 < argc) {
      game_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--cd-dir") && i + 1 < argc) {
      cd_dir = argv[++i];
    } else if (!std::strcmp(argv[i], "--")) {
      for (i++; i < argc; i++) {
        if (!game_args.empty()) game_args += ' ';
        game_args += argv[i];
      }
    } else {
      std::fprintf(stderr, "usage: %s [--game-dir DIR] [-- GAME ARGS...]\n", argv[0]);
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
  return recomp::RunProcess(game, exe, game_dir, game_args);
}
