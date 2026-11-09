#include "game/race_setup.h"

#include <cstdio>
#include <cstdlib>

#include "red/ini_text.h"

namespace vracer {

RaceSetup g_race;

namespace {

bool ParseCamera(const std::string& v, ChaseCamera* out) {
  float h, d;
  if (std::sscanf(v.c_str(), "%f,%f", &h, &d) != 2) return false;
  out->height = h;
  out->distance = d;
  return true;
}

}  // namespace

void LoadRaceSettings(const std::string& path) {
  red::IniText ini;
  if (!ini.Load(path)) return;
  for (auto& [k, v] : ini.Entries("Cameras")) {
    ChaseCamera cam;
    if (!ParseCamera(v, &cam)) continue;
    if (red::Lower(k) == "trailer") {
      g_race.trailer_camera = cam;
      g_race.has_trailer_camera = true;
    } else {
      g_race.cameras[std::atoi(k.c_str())] = cam;
    }
  }
}

}  // namespace vracer
