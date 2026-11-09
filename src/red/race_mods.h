// RED engine: the race mod layer and the vehicle swap, applied by the race
// binary at start, before the game reads its files.
//
// The race runs on SwARc, which reads a file from the per-user overlay
// (<data dir>/install/...) before the CD. Each start the mod layer
// rebuilds those overlay copies from the CD:
//   - <mods>/<File>.ini: keys laid over the CD's File.ini (only the keys
//     you want to change or add). Files the game or the menu write
//     themselves (game.ini, raser.ini) are live: there a mod only adds the
//     keys that are missing.
//   - any other file in <mods> (textures, models): copied as is. A name
//     that is not in the CD's archive is loaded from the overlay.
//   - the vehicle swap (game.ini [PLAYER] VEHICLE): the player keeps his
//     car slot (CAR: character, portrait, opponents, results) but drives
//     another car's vehicle: model, sound, wheels and lights (the car
//     table row), mass, physics and car data records, roof luggage.
// Files the layer wrote last time and not this time are removed, so
// taking a mod out restores the CD.
#pragma once

#include <string>
#include <vector>

namespace red {

struct RaceModConfig {
  std::string game_dir;              // the CD's data folder
  std::string overlay_dir;           // SwARc's <data dir>/install
  std::vector<std::string> mod_dirs; // later dirs win
  std::vector<std::string> live_files = {"game.ini", "raser.ini"};
  // Files in the mod folders that are settings of the port, not game files.
  std::vector<std::string> port_files;
  // Car table: "<i>=RefName,AnimName,DriverName,SoundType,Mass,CarType,Price"
  // per difficulty section.
  std::string car_table = "Car_cfg.ini";
  std::vector<std::string> car_sections = {"league0", "league1", "league2"};
  // Per car records, one file per difficulty: name pattern ("cars%d.dat")
  // and record size.
  struct RecordFile {
    std::string pattern;
    int record_size;
  };
  std::vector<RecordFile> record_files;
};

struct PlayerSetup {
  int car = -1;      // game.ini [PLAYER] CAR (the slot)
  int vehicle = -1;  // the car he drives (= car without a swap)
  std::string trailer;      // [PLAYER] TRAILER: RefName or NONE
  bool trailer_set = false;  // false: the game's own choice
};

// Applies the mod layer and the swap; returns the player's setup.
PlayerSetup ApplyRaceMods(const RaceModConfig& cfg);

}  // namespace red
