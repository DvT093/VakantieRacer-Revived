// The Vakantie Racer screens, one class per original Delphi form
// (TfrmXxx in Menu.exe). Each lives in its own screen_*.cpp.
#pragma once

#include <functional>
#include <string>

#include "engine/video.h"
#include "gfx_settings.h"
#include "vracer/game.h"

namespace vr {

// First screen after start: the intro movie (when `intro`), then the
// player check and the main menu.
void Start(Game& g, bool intro);
void BackFromIntro(Game& g);
// Stat dots: `value` of [BITMAPS] 0, then -delta of 1 or +delta of 2, 13 px apart.
void DrawDots(Game& g, SDL_Surface* canvas, int x, int y, int value, int delta);
// "%s" substitution as in Delphi's Format for the menu strings.
std::string Fmt(const std::string& f, const std::string& a);

class MainScreen : public VrScreen {  // TfrmMainScreen (MAINSCRE.bmp)
 public:
  explicit MainScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
};

class DifficultyScreen : public VrScreen {  // TfrmDifficulty
 public:
  explicit DifficultyScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
};

class QuickRaceScreen : public VrScreen {  // TfrmQuickRace
 public:
  explicit QuickRaceScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;
  int CharIndex() const { return char_; }
  int TrackIndex() const { return track_; }

 private:
  void SetChar(int i);
  void SetTrack(int i);
  // Port: the entries are the characters, then the bonus cars (the
  // presenter in the Ferrari as in the original, the Z3 and DeLorean when
  // open); the character row walks them. car_ = the bonus car of the
  // bonus entry. The caravan thumbnails (thmTrailer0-3) pick the trailer
  // (-1 = the car's own); locked_: the entry is not open.
  std::vector<int> Entries();  // car numbers, characters first
  void SetEntry(int k);
  void Step(int dir);
  void SetTrailer(int trailer);
  int char_ = 0, track_ = 0, car_ = -1, trailer_ = -1;
  bool locked_ = false;
};

class ResultsScreen : public VrScreen {  // TfrmResults
 public:
  explicit ResultsScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;
};

// Popups (TGameMenuPopup / TCaptionForm descendants).
class StartInfoPopup : public VrScreen {  // TfrmStartInfo, POP_STRT.bmp
 public:
  explicit StartInfoPopup(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
};

class PlayerNamePopup : public VrScreen {  // TfrmPlayerName
 public:
  explicit PlayerNamePopup(Game& g);
  // Background (POP_SPLR new player, POP_ROND lap record), text and name.
  void Setup(const std::string& bmp, const std::string& text, const std::string& name);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  std::string name;

 private:
  std::string text_, initial_, default_text_;
};

// Vakantie Race screens (screen_vacation.cpp).
class CharacterScreen : public VrScreen {  // TfrmCharacter (CHARACTE.bmp)
 public:
  explicit CharacterScreen(Game& g);
  void OnShow() override;
  void OnHide() override;
  void OnClick(davi::Widget& w) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;

 private:
  void SetChar(int i);
  int char_ = 0;
};

class TrackScreen : public VrScreen {  // TfrmTrack (TRACK.bmp)
 public:
  explicit TrackScreen(Game& g);
  void OnShow() override;
  void OnHide() override;
  void OnClick(davi::Widget& w) override;

 private:
  void SetTrack(int i);
  void Cycle();
  int track_ = 0, image_ = 0;
};

class GarageScreen : public VrScreen {  // TfrmGarage (GARAGE.bmp)
 public:
  explicit GarageScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;

 private:
  void UpdateButtons();
  void ShowItem();
  void ShowTrailer();  // port: the caravan thumbnails thmTrailer0-3
  void Buy();
  CharResult& Res();
  int item_ = 0;     // 0 none, 1 tune-up, 2 repair, 3 turbo oil
  int tuneup_ = 0;   // [TUNEUPS] index for item 1
};

class TrackIntroScreen : public VrScreen {  // TfrmTrackIntro (TRACKINT.bmp)
 public:
  explicit TrackIntroScreen(Game& g);
  void OnShow() override;
  void OnHide() override;
  void OnClick(davi::Widget& w) override;
};

// After a Vakantie Race race (screen_after.cpp).
class HostScreen : public VrScreen {  // TfrmHost (HOST.bmp)
 public:
  explicit HostScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
};

class TrackWinnerScreen : public VrScreen {  // TfrmTrackWinner (TRACKWIN.bmp)
 public:
  explicit TrackWinnerScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
};

class WheelScreen : public VrScreen {  // TfrmWheelOfFortune (WHEELOFF.bmp)
 public:
  explicit WheelScreen(Game& g);
  void OnShow() override;
  void OnHide() override;
  void OnClick(davi::Widget& w) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;

 private:
  void Step();
  void Award();
  void Buttons();
  int state_ = 1, light_ = 0;
  double interval_ = 50, factor_ = 1.15;
};

class WinRaceScreen : public VrScreen {  // TfrmWinRace (WINRACE.bmp)
 public:
  explicit WinRaceScreen(Game& g);
  void OnShow() override;
  void OnHide() override;
  void OnClick(davi::Widget& w) override;
  void Tick(double dt) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;

 private:
  davi::Video movie_;  // the character's AVI, looped in pnlMovie
};

class IntroScreen : public VrScreen {  // TfrmIntro: menu.ini INTROAVI
 public:
  explicit IntroScreen(Game& g);
  bool Play(const std::string& path);
  void OnClick(davi::Widget& w) override;
  bool OnKey(SDL_Keycode k) override;
  void Tick(double dt) override;
  void Paint(SDL_Surface* canvas, int ox, int oy) override;

 private:
  void Done();
  davi::Video movie_;
  bool done_ = false;
};

class Ferrari2Popup : public VrScreen {  // TfrmFerrari2 (POP_GWIN.bmp)
 public:
  explicit Ferrari2Popup(Game& g);
  void OnClick(davi::Widget& w) override;
};

// Player management (screen_player.cpp).
class PlayerScreen : public VrScreen {  // TfrmPlayer (PLAYER.bmp)
 public:
  explicit PlayerScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;

 private:
  void Update();
  std::vector<int> others_;  // player indexes on btn1..4
  std::vector<Player> backup_;
  int backup_current_ = -1;
};

// Options (screen_options.cpp).
class OptionsScreen : public VrScreen {  // TfrmOptions (OPTIONS.bmp)
 public:
  explicit OptionsScreen(Game& g);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;

 private:
  void Video();
  void Control(int device);
  Player temp_;          // edited copy, stored on OK
  vracer::GfxSettings gfx_;  // graphics.ini, stored on OK (Gevorderd)
  davi::Sound sample_;   // wavIrritatingAsHell ("trin.wav")
};

class KeySettingsPopup : public VrScreen {  // TfrmKeySettings (POP_KEYS)
 public:
  explicit KeySettingsPopup(Game& g);
  void Edit(const int* keys);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  bool OnKey(SDL_Keycode k) override;
  int keys[7] = {};  // up down left right turbo handbrake horn

 private:
  void Captions();
  int waiting_ = -1;
};

class SensitivityPopup : public VrScreen {  // TfrmJoystickSettings / TfrmWheelSettings
 public:
  explicit SensitivityPopup(Game& g);
  void Edit(bool wheel, int value);
  void OnClick(davi::Widget& w) override;
  int value = 50;
};

// Opties > Gevorderd: de beeld-upgrades van de port (graphics.ini).
class DisplaySettingsPopup : public VrScreen {  // TfrmDisplaySettings
 public:
  explicit DisplaySettingsPopup(Game& g);
  ~DisplaySettingsPopup() override;
  void Edit(const vracer::GfxSettings& s);
  void OnShow() override;
  void OnClick(davi::Widget& w) override;
  vracer::GfxSettings settings;

 private:
  void Captions();
  SDL_Surface* bg_ = nullptr;
};

// TfrmMenuDialog: message box. Buttons: 1 OK, 2 Annuleer, 4 Ja, 8 Nee.
// The result is 1 (OK), 2 (Annuleer), 6 (Ja) or 7 (Nee).
class MenuDialog : public VrScreen {
 public:
  explicit MenuDialog(Game& g);
  void Open(const std::string& text, const std::string& title, int buttons, std::function<void(int)> done);
  void OnClick(davi::Widget& w) override;
};
void Message(Game& g, const std::string& text, std::function<void(int)> done = nullptr);

// Asks a name (TfrmPlayerName, POP_SPLR) until one is given, then creates
// the player (CreateNewPlayer) and calls done.
void AskForNewPlayer(Game& g, std::function<void()> done);

// Starts the race (StartInfo first when enabled), takes in the results
// (Game::last, ranking, playerpos, rrflags) and calls `after`.
void StartRace(Game& g, int char_index, int track_index, std::function<void()> after);
// The car slot of a character (his own car; the bonus entry: the Ferrari).
int SlotCar(Game& g, int char_index);
// The car of a Quick Race entry: a character's own car, or for the bonus
// entry (the presenter) the chosen bonus car (5 Ferrari, 6 Z3, 7 DeLorean).
int BonusEntryCar(Game& g, int char_index, int car);
// The trailer for the race (-1 = the standard trailer of his car slot).
int ChosenTrailer(Game& g, int char_index, int trailer);  // index into Game::trailers
// The caravan thumbnails thmTrailer<i> (layout Class=TDaviThumb, one per
// Game::trailers entry): picture (or the name when there is none), the
// chosen one checked, all enabled or not.
void ShowTrailerChoice(Game& g, davi::Form& form, int char_index, int trailer, bool enabled = true);
// The trailer index of a thumbnail thmTrailer<i>, or -1 for other widgets.
int TrailerThumb(const davi::Widget& w);
void TakeResults(Game& g, int char_index, int track_index);

}  // namespace vr
