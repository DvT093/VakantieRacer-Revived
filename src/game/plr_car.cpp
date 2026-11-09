// plr_car.cpp (D:\Work\RED\Game\prog\plr_car.cpp): the player's car.
// Replaced: init_player_car gives the player the trailer chosen in the
// menu (game.ini [PLAYER] TRAILER: a car table RefName such as CARAVAN, or
// NONE). Without that key the game's own rule stays: car 0 (Ami) the
// caravan, 2 (2CV) the hippie caravan, 4 (G-klasse) the jetski.

#include <strings.h>

#include "game/race_setup.h"
#include "x86/runtime.h"

namespace vracer {

extern bool g_in_player_init, g_trailer_handled;
uint32_t AttachTrailer(recomp::Ctx& c, uint32_t tractor, const std::string& name);

}  // namespace vracer

namespace recomp::gen {

void f_00439FA0_lifted(::recomp::Ctx& c);

// 0x439FA0 init_player_car(): makes the player's car (RACER_<CAR> from the
// car table) at its start position. Its trailer comes from a fixed if-chain
// on the car number 0x54CFF4 that calls add_trailer (0x4204B0, see
// car_trailer.cpp) and keeps the trailer object in 0x54CFA4. A car without
// a trailer of its own gets the chosen one right after.
void f_00439FA0(::recomp::Ctx& c) {
  vracer::g_in_player_init = true;
  vracer::g_trailer_handled = false;
  vracer::g_player_trailer = false;
  f_00439FA0_lifted(c);
  vracer::g_in_player_init = false;
  const red::PlayerSetup& p = vracer::g_race.player;
  if (vracer::g_trailer_handled || !p.trailer_set || !strcasecmp(p.trailer.c_str(), "NONE")) return;
  const uint32_t eax = c.eax;
  const uint32_t trailer = vracer::AttachTrailer(c, Rd32(0x54CFF0), p.trailer);
  if (trailer < 0x80000000u) {
    Wr32(0x54CFA4, trailer);
    vracer::g_player_trailer = true;
  }
  c.eax = eax;
}

}  // namespace recomp::gen
