// What the player drives in this race and the port's own race settings
// (mods/race/vracer.ini), set by main.cpp before the game starts and read
// by the replaced game functions in src/game/.
#pragma once

#include <map>
#include <string>

#include "red/race_mods.h"

namespace vracer {

struct ChaseCamera {
  float height = 38, distance = -120;
};

struct RaceSetup {
  red::PlayerSetup player;
  // mods/race/vracer.ini [Cameras]: <car>=height,distance per vehicle, and
  // "trailer" for a car pulling a trailer it does not have by itself.
  std::map<int, ChaseCamera> cameras;
  bool has_trailer_camera = false;
  ChaseCamera trailer_camera;
};

extern RaceSetup g_race;
// The player's car pulls a trailer in this race (plr_car.cpp).
extern bool g_player_trailer;

// Reads mods/race/vracer.ini (missing = no settings).
void LoadRaceSettings(const std::string& path);

}  // namespace vracer
