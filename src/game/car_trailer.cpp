// car_trailer.cpp (D:\Work\RED\Game\prog\car_trailer.cpp): trailers.
// Replaced: when init_player_car (plr_car.cpp) hangs the game's own
// trailer behind the player's car, the trailer chosen in the menu
// (game.ini [PLAYER] TRAILER) is used instead, or none.

#include <strings.h>

#include "game/race_setup.h"
#include "x86/runtime.h"

namespace recomp::gen {

void f_004204B0_lifted(::recomp::Ctx& c);

}  // namespace recomp::gen

namespace vracer {

bool g_player_trailer = false;
bool g_in_player_init = false;  // plr_car.cpp
bool g_trailer_handled = false;

constexpr uint32_t kPlayerObject = 0x54CFF0;  // plr_car.cpp

// 0x4204B0 add_trailer(int tractor, std::string name) (cdecl) with a name
// of our own. The string goes by value (MSVC 6: allocator, _Ptr, _Len,
// _Res); its text lives on the guest stack with a reference count of 1,
// so the callee's destructor only decrements it. Returns the trailer's
// object number, or 0xFFFFFFFF.
uint32_t AttachTrailer(recomp::Ctx& c, uint32_t tractor, const std::string& name) {
  using recomp::Wr32;
  const uint32_t saved = c.esp;
  c.esp = (c.esp - 64 - uint32_t(name.size())) & ~3u;
  const uint32_t text = c.esp;
  recomp::Wr8(text, 1);
  recomp::PutStr(text + 1, name);
  const uint32_t args[5] = {tractor, 0, text + 1, uint32_t(name.size()), uint32_t(name.size())};
  for (int i = 4; i >= 0; i--) {
    c.esp -= 4;
    Wr32(c.esp, args[i]);
  }
  c.esp -= 4;
  Wr32(c.esp, 0);  // return address (the lifted ret only pops it)
  recomp::gen::f_004204B0_lifted(c);
  const uint32_t r = c.eax;
  c.esp = saved;
  return r;
}

}  // namespace vracer

namespace recomp::gen {

// 0x4204B0 add_trailer(int tractor, std::string name): looks the name up in
// the car table (Car_cfg.ini RefName, e.g. CARAVAN), makes the trailer
// object at the tractor and couples it (0x41EC80, hitch from both models'
// bounding boxes). Only the player's own trailer during init_player_car is
// changed; the opponents keep theirs.
void f_004204B0(::recomp::Ctx& c) {
  const uint32_t tractor = Rd32(c.esp + 4);
  if (!vracer::g_in_player_init || tractor != Rd32(vracer::kPlayerObject)) {
    f_004204B0_lifted(c);
    return;
  }
  vracer::g_trailer_handled = true;
  const red::PlayerSetup& p = vracer::g_race.player;
  const std::string given = GStr(Rd32(c.esp + 0xC));
  if (!p.trailer_set || !strcasecmp(given.c_str(), p.trailer.c_str())) {
    f_004204B0_lifted(c);
  } else if (!strcasecmp(p.trailer.c_str(), "NONE")) {
    c.eax = 0xFFFFFFFF;  // the game's string is not freed: a few bytes, once per race
    c.esp += 4;          // ret
  } else {
    c.eax = vracer::AttachTrailer(c, tractor, p.trailer);
    c.esp += 4;  // ret
  }
  vracer::g_player_trailer = c.eax < 0x80000000u;
}

}  // namespace recomp::gen
