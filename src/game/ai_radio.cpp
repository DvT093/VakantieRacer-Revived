// ai_radio.cpp (D:\Work\RED\Game\prog\ai_radio.cpp): the race radio, one
// object at 0x516AE0 that plays the cop radio and the racers' voices.
// Per car number <n> (0 to the number of racers, 0x5166C8) 0x4121B0 loads
// RAC<n>C<i>.WAV (collision), RAC<n>P<i>.WAV (passing, and the T key) and
// HORN<n> into three tables of vectors, 0x10 bytes per car, at +0x7C, +0xEC
// and +0x15C. On T, 0x43AFF5 sends event 9: the player's own RAC<n>P
// lines, n = his car number 0x54CFF4 (his character, never the trailer).
// Replaced: the bonus cars (5 Ferrari, 6 Z3, 7 DeLorean) are driven by the
// presenter, as the original's Ferrari portrait (PLAYER_5) shows; the CD
// has no RAC5..7 lines, so the port loads RAC5P<i>.WAV (the presenter's
// cheers, cut from his menu lines by tools/make_mod_assets.py into
// mods/race/local) into the passing table of car 5 and plays those on T in
// any bonus car. VRACER_LOG_VOICE=1 logs each voice request.

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "x86/runtime.h"

namespace recomp::gen {

void f_004121B0_lifted(::recomp::Ctx& c);
void f_00412230_lifted(::recomp::Ctx& c);
void f_00412990(::recomp::Ctx& c);
void f_00411E70(::recomp::Ctx& c);

}  // namespace recomp::gen

namespace {

using recomp::Rd32;
using recomp::Wr32;

constexpr uint32_t kRacerCount = 0x5166C8;  // the loader loads cars 0..this
constexpr uint32_t kPlayerCar = 0x54CFF4;
constexpr uint32_t kPassTable = 0xEC;       // RAC<n>P vectors in the radio
constexpr int kPresenterCar = 5;            // fits the passing table (7 slots)
constexpr int kFirstBonusCar = 5;
bool g_presenter_loaded = false;

// A thiscall into lifted code: ecx = self, the arguments pushed right to
// left, a dummy return address; the callee pops them (ret n).
uint32_t ThisCall(recomp::Ctx& c, void (*f)(recomp::Ctx&), uint32_t self, std::initializer_list<uint32_t> args) {
  const uint32_t saved = c.esp, ecx = c.ecx;
  for (auto it = args.end(); it != args.begin();) {
    c.esp -= 4;
    Wr32(c.esp, *--it);
  }
  c.esp -= 4;
  Wr32(c.esp, 0);
  c.ecx = self;
  f(c);
  const uint32_t r = c.eax;
  c.esp = saved;
  c.ecx = ecx;
  return r;
}

}  // namespace

namespace recomp::gen {

// 0x4121B0 load_racer_voices(radio) (fastcall): per car 0..0x5166C8 its
// collision, passing and horn lines. Then (port) the presenter's lines in
// the passing table of car 5, when the race has no car 5 of its own.
void f_004121B0(::recomp::Ctx& c) {
  const uint32_t radio = c.ecx;
  f_004121B0_lifted(c);
  g_presenter_loaded = false;
  if (int32_t(Rd32(kRacerCount)) >= kPresenterCar) return;
  const uint32_t eax = c.eax;
  const uint32_t table = radio + kPassTable + kPresenterCar * 0x10;
  ThisCall(c, f_00412990, table, {0x10});  // the vector, as the loader does
  int n = 0;
  while (n < 16 && ThisCall(c, f_00411E70, radio, {uint32_t(kPresenterCar), uint32_t(n)}) != 0xFFFFFFFFu) n++;
  g_presenter_loaded = n > 0;
  if (std::getenv("VRACER_LOG_VOICE"))
    std::fprintf(stderr, "voice: presenter lines RAC%dP0-%d loaded: %d\n", kPresenterCar, n - 1, n);
  c.eax = eax;
}

// 0x412230 radio_event(radio, int event, int object) (thiscall): queues one
// radio message when the radio is idle. Events 6/7/8 take a racer object
// (collision / passed / passing, the lines of that racer's car), 9 is the
// T key (the player's lines, car number 0x54CFF4).
void f_00412230(::recomp::Ctx& c) {
  static const bool log = std::getenv("VRACER_LOG_VOICE") != nullptr;
  const uint32_t radio = c.ecx;
  const uint32_t event = Rd32(c.esp + 4), object = Rd32(c.esp + 8);
  const uint32_t pending = Rd32(radio + 0x1DC), state = Rd32(radio + 0x1D8);
  const uint32_t car = Rd32(kPlayerCar);
  const bool presenter = event == 9 && int32_t(car) >= kFirstBonusCar;
  if (presenter) {
    if (!g_presenter_loaded) {  // no lines: say nothing (the original's Ferrari)
      c.esp += 12;              // ret 8
      return;
    }
    Wr32(kPlayerCar, kPresenterCar);
  }
  f_00412230_lifted(c);
  if (presenter) Wr32(kPlayerCar, car);
  if (!log) return;
  // The new state names the table it plays from: 0xF the collision lines
  // (+0x70), 0x11 and 0x15 the passing lines (+0x74), 0x13 the horn (+0x78).
  const uint32_t now = Rd32(radio + 0x1D8);
  const int k = now == 0xF ? 0 : now == 0x11 || now == 0x15 ? 1 : now == 0x13 ? 2 : -1;
  if (state != 0 || k < 0) {
    std::fprintf(stderr, "voice: event %u object %u: radio busy (state %u, pending %u)\n", event, object, state, pending);
    return;
  }
  static const char* kind[3] = {"C", "P", "HORN"};
  static const uint32_t base[3] = {0x7C, kPassTable, 0x15C};
  const uint32_t slot = (Rd32(radio + 0x70 + 4 * k) - radio - base[k]) / 0x10;
  std::fprintf(stderr, "voice: event %u object %u -> RAC%u%s lines (player car %u)\n", event, object, slot, kind[k],
               car);
}

}  // namespace recomp::gen
