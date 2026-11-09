// Vakantie Racer menu: game data and shared state of the flow.
// Everything comes from the game's own ini files (static.ini, menu.ini,
// MenuCaps.ini) with the mod layer (mods/menu/) on top: adding a character,
// a track or a prize is an ini entry, not code.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/app.h"
#include "launcher/race_launcher.h"

namespace vr {

// static.ini [charN]
struct Character {
  int index = 0;  // N of [charN]
  std::string section;
  std::string name, bitmap, carbitmap, qrbmp, wofbmp, resultsbmp;
  std::string hostwinbmp, hostlosebmp, hostqualbmp;
  std::string introwav, outrowav, chosenwav, rndwinprizewav, rndwinnothingwav;
  std::string wintext, winracetext, story, video;
  int indexingame = 0;  // game.ini [PLAYER] car
  int speed = 0, accell = 0, handling = 0, brakes = 0;
};

// static.ini [carN] (mods/menu): a car of the game's car table that the
// autoselector offers. N is the car number (game.ini [PLAYER] CAR/VEHICLE).
struct Car {
  int index = 0;
  std::string name;
  std::string carbitmap;  // the car alone (CAR_*.bmp, 297x227)
  int speed = 0, accell = 0, handling = 0, brakes = 0;
  std::string trailer;  // the game's own trailer for this car slot ("" none)
  bool bonus = false;   // only once every character won the Vakantie Race (or Alles vrij)
};

// static.ini [TRAILERS] (mods/menu): what a car can pull. ref is the car
// table RefName the race takes (game.ini [PLAYER] TRAILER), NONE for none.
struct Trailer {
  std::string ref, name, bitmap;
};

// static.ini [TRACKn]
struct Track {
  int index = 0;
  std::string name, type, length, bitmap, story, wintext, qualifytext, losetext, introwav;
  int nrlaps = 1, indexingame = 0;
  std::vector<int> qualify;  // QUALIFYPOSITION per difficulty (0 = easy)
};

// static.ini [TUNEUPS] SPECSn
struct TuneUp {
  std::string name, text;
  int price = 0, dspeed = 0, dacc = 0, dbrakes = 0, dhandl = 0;
};

// static.ini [wofprizes]
struct Prize {
  bool istuneup = false;
  int value = 0;
  std::string wintext, imagefile;
};

// menu.ini [HIGHSCORES]: best lap per track.
struct Record {
  int time_ms = 0;
  std::string name;
};

// Per character progress in the Vakantie Race (TCharResult).
struct CharResult {
  int tracktodrive = 0;  // next track to drive
  int money = 0;         // Sunnies
  bool won = false;      // won the final with this character
  int damage = 0, turbo = 0;
  bool tuneup[4] = {};   // Motor, Banden, ABS, Versnellingsbak (one race)
  // Port: the car he drives (-1 = his own) and the trailer (-1 = the
  // standard one of his car, else an index into Game::trailers).
  int car = -1, trailer = -1;
};

// One player (the original kept these in binary save files SaveGame<N>.rcr;
// the native menu uses menu_players.ini in the user data folder).
struct Player {
  std::string name;
  // TSettings, defaults from static.ini [DEFAULTSETTINGS].
  int difficulty = 1;  // 0 easy, 1 normal, 2 hard
  bool show_start_info = true;
  int music_volume = 35, fx_volume = 75, voices_volume = 75;
  int resolution = 3, device = 1;  // 1 keyboard, 2 joypad, 3 joystick, 4 wheel
  int keys[7] = {38, 40, 37, 39, 17, 18, 32};  // up down left right turbo handbrake horn (VK)
  int joy_sensitivity = 50, wheel_sensitivity = 50;
  int gamma = 0;
  bool alpha = true, bilinear = true, waitvbl = false;
  // Quick Race selection; car/trailer as in CharResult.
  int qr_char = 0, qr_track = 0;
  int qr_car = -1, qr_trailer = -1;
  // Port: "Alles vrij" (Opties): the bonus cars and every Vakantie Race
  // track open, without changing the progress itself.
  bool unlock_all = false;
  // Vakantie Race
  int current_char = 4, current_track = 0;
  std::vector<CharResult> results;  // one per character
  CharResult& Res(int c) {
    if (c >= int(results.size())) results.resize(c + 1);
    return results[c];
  }
};

// After a race (TData+0xd4 "RR" flags, 0x48e17c / 0x48e374).
enum RaceFlags { kQualify = 1, kWinRace = 2, kFortune = 4, kWinAll = 8, kBestLap = 16 };
struct Ranked {
  int character = -1;  // index into Game::chars (-1 = bonus car)
  int time_ms = 0;
};

class Game;

// Base of the Vakantie Racer screens: a form plus a back pointer to the game.
class VrScreen : public davi::Screen {
 public:
  VrScreen(Game& g, const std::string& cls);
  Game& game;
  void Update(double dt) final;
  void PaintOver(SDL_Surface* canvas, int ox, int oy) final;
  // For the screens: per-frame update and extra drawing.
  virtual void Tick(double dt) { (void)dt; }
  virtual void Paint(SDL_Surface* canvas, int ox, int oy) { (void)canvas; (void)ox; (void)oy; }

 private:
  SDL_Surface* bulb_ = nullptr;
  double bulb_t_ = 0;
  int bulb_frame_ = 0;
};

class Game {
 public:
  Game(davi::App& app, red::Install install, std::vector<std::string> race_argv);
  bool Load();

  davi::App& app;
  davi::Assets& assets;
  red::RaceLauncher launcher;
  davi::LayeredIni stat;  // static.ini
  davi::LayeredIni menu;  // menu.ini (read)
  std::vector<Character> chars;
  std::vector<Car> cars;          // by car number (missing numbers: index -1)
  std::vector<Trailer> trailers;  // [0] = none
  std::vector<Track> tracks;
  std::vector<TuneUp> tuneups;
  std::vector<Prize> prizes;
  std::vector<Record> records;
  std::vector<Player> players;
  int current_player = -1;
  std::string help_file;  // MenuData/VRacer.hlp ("" = no help buttons)
  // Sounds embedded in the EXE's data module, played as "__<name>".
  std::map<std::string, davi::Sound> embedded;

  // Quick Race (1) or Vakantie Race (2), like the original's race mode.
  int mode = 0;
  // Last race: ranking by time, the player's place (0 based) and RR flags.
  std::vector<Ranked> ranking;
  int playerpos = 0;
  int rrflags = 0;
  red::RaceResult last;
  void NewPlayer(const std::string& name);
  int CharByIndexInGame(int car) const;  // -1 if none

  // Autoselector and trailers (port).
  const Car* CarInfo(int car) const;   // nullptr if [carN] is missing
  // Every character won the Vakantie Race (Menu.exe 0x46c808), or Alles vrij.
  bool BonusUnlocked();
  bool CarAvailable(int car);
  // The next/previous available car after `car` (wraps).
  int NextCar(int car, int dir);
  // Index into trailers of a RefName (0 = none if unknown).
  int TrailerIndex(const std::string& ref) const;
  // The car's picture alone, scaled to w x h (cached; nullptr if missing).
  SDL_Surface* CarPicture(int car, int w, int h);
  // A trailer's small picture (nullptr if missing).
  SDL_Surface* TrailerPicture(int trailer);

  std::string Str(int n) const;              // menu.ini [MENUSTRINGS]
  std::string Ordinal(int pos1) const;       // [ordinalnumbers]: 1 -> "1e"
  std::string FormatTime(int ms) const;      // [FORMAT] TIME
  Player* Cur() { return current_player >= 0 && current_player < int(players.size()) ? &players[current_player] : nullptr; }
  void SavePlayers();
  void SaveRecords();
  // Plays a voice line; `base` + random 0..n-1 when n > 0 ("jac_hlp", 3).
  void Voice(const std::string& base, int n = 0);
  void Sound(const std::string& name);
  void StartMusic();

  // Screens (created on first use, like Delphi singleton forms).
  template <class T> T* Screen() {
    auto& p = screens_[typeid(T).name()];
    if (!p) p = std::make_unique<T>(*this);
    return static_cast<T*>(p.get());
  }
  // ShowScreen: remembers where it came from (for Back()).
  void Goto(davi::Screen* s);
  void Back();
  // 0 until the first race of this session, then the race mode (the
  // original's PREVSTATE, as the menu restarted after every race).
  int prevstate = 0;

 private:
  void PlayerIo(davi::IniFile& f, const std::string& sec, Player& p, bool save);
  std::map<std::string, std::unique_ptr<davi::Screen>> screens_;
  std::map<std::string, SDL_Surface*> scaled_;
  std::map<davi::Screen*, davi::Screen*> prev_;
  std::string players_path_;
};

void RegisterSkin(davi::Skin& skin);

}  // namespace vr
