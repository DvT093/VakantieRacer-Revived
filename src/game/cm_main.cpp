// cm_main.cpp (D:\Work\RED\Game\prog\cm_main.cpp): the game's cameras.
// Replaced: the player's chase camera follows the car he drives (with a
// vehicle swap) and has a setting per car (mods/race/vracer.ini [Cameras]),
// so the bonus cars and a car with a trailer get a proper camera.

#include <cstring>

#include "game/race_setup.h"
#include "x86/runtime.h"

namespace recomp::gen {

void f_004270B0_lifted(::recomp::Ctx& c);

}  // namespace recomp::gen

namespace {

using recomp::Rd32;
using recomp::Wr32;

// Globals of cm_main.cpp and the object table.
constexpr uint32_t kCameraMode = 0x4F65E0;     // last mode set
constexpr uint32_t kTargetObject = 0x4F65E4;   // object the camera follows
constexpr uint32_t kActiveCamera = 0x519C58;   // index into the camera array
constexpr uint32_t kCameras = 0x5696F8;        // camera array, 0x178 bytes each
constexpr uint32_t kObjects = 0x563D80;        // object array, 0xE4 bytes each
constexpr uint32_t kDampening = 0x519CB0;      // 3 floats, the chase spring

uint32_t Bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// The chase camera of modes 5, 6, 7, 9 and 11 (0x4270B0 case 5 and the
// code after 0x42710C) with our own height and distance: camera offset
// behind the car, looking at a point 10 above it, spring dampening.
void ChaseCamera(recomp::Ctx& c, uint32_t mode, float height, float distance) {
  Wr32(kCameraMode, mode);
  uint32_t cam = Rd32(kCameras) + Rd32(kActiveCamera) * 0x178;
  uint32_t obj = Rd32(kObjects) + Rd32(kTargetObject) * 0xE4;
  c.ecx = cam;  // SetPositionOffset(vec3{0, height, distance})
  recomp::CallGuest(c, 0x47F310, {0, Bits(height), Bits(distance)});
  recomp::CallGuest(c, 0x426430, {0});
  Wr32(cam + 0x80, 0);
  c.ecx = cam;  // LockTarget(object model, vec3{0, 10, 0})
  recomp::CallGuest(c, 0x47F390, {Rd32(obj + 0x20), 0, Bits(10.0f), 0});
  Wr32(obj + 0x50, 1);
  c.ecx = cam;  // SetDampening(vec3)
  recomp::CallGuest(c, 0x47F210, {Rd32(kDampening), Rd32(kDampening + 4), Rd32(kDampening + 8)});
}

// Cars that pull a trailer by themselves (plr_car.cpp: Ami, 2CV, G-klasse):
// their own chase camera is already made for it.
bool PullsTrailerByItself(int car) { return car == 0 || car == 2 || car == 4; }

}  // namespace

namespace recomp::gen {

// 0x4270B0 set_game_camera(mode) (cdecl). Modes 5 + car are the chase
// cameras per car (0x429EE0 at the start, 0x426690 on key 2); mode 12 is
// not a chase camera but the cockpit-like camera 0x4276C0, which is why
// car 7 (the DeLorean) originally sat inside its own car.
void f_004270B0(::recomp::Ctx& c) {
  const uint32_t mode = Rd32(c.esp + 4);
  const red::PlayerSetup& p = vracer::g_race.player;
  int car = int(mode) - 5;
  if (car < 0 || car != p.car || p.vehicle < 0) {
    f_004270B0_lifted(c);
    return;
  }
  int vehicle = p.vehicle;
  if (vracer::g_player_trailer && !PullsTrailerByItself(vehicle) && vracer::g_race.has_trailer_camera) {
    ChaseCamera(c, mode, vracer::g_race.trailer_camera.height, vracer::g_race.trailer_camera.distance);
    c.esp += 4;  // ret
    return;
  }
  auto it = vracer::g_race.cameras.find(vehicle);
  if (it != vracer::g_race.cameras.end()) {
    ChaseCamera(c, mode, it->second.height, it->second.distance);
    c.esp += 4;  // ret
    return;
  }
  // The original camera of the car he drives.
  Wr32(c.esp + 4, uint32_t(vehicle + 5));
  f_004270B0_lifted(c);
  Wr32(c.esp + 4, mode);
  Wr32(kCameraMode, mode);
}

}  // namespace recomp::gen
