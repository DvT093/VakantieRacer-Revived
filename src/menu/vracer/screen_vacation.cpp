// Vakantie Race, before the race: TfrmCharacter, TfrmTrack, TfrmGarage
// and TfrmTrackIntro.
#include <cmath>

#include "vracer/screens.h"

namespace vr {

// ---------------------------------------------------------------- Character

CharacterScreen::CharacterScreen(Game& g) : VrScreen(g, "TfrmCharacter") {}

void CharacterScreen::OnShow() {
  Player* p = game.Cur();
  int n = int(game.chars.size());
  SetChar(p ? ((p->current_char % n) + n) % n : 0);
  game.app.SetFocus(form.Get("btnOk"));
}

void CharacterScreen::OnHide() { game.app.CancelTimers(this); }

// Name, story, portrait, car; a sun per track already reached (all six
// once the character won); the intro voice after one second (tmrSound).
void CharacterScreen::SetChar(int i) {
  char_ = i;
  const Character& c = game.chars[i];
  form.SetCaption("lblTitle", c.name);
  form.SetCaption("lblTextBlock", c.story);
  form.SetPicture("imgCharacter", game.assets.Bitmap(c.bitmap));
  form.SetPicture("imgCar", game.assets.Bitmap(c.carbitmap));
  Player* p = game.Cur();
  CharResult r = p ? p->Res(i) : CharResult{};
  for (int s = 0; s < 6; s++) form.SetVisible("imgSun" + std::to_string(s), s < r.tracktodrive || r.won);
  game.app.CancelTimers(this);
  std::string wav = c.introwav;
  Game& g = game;
  game.app.After(1000, [&g, wav]() { g.Voice(wav); }, this);
}

void CharacterScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* bars = form.Get("imgBars");
  if (!bars) return;
  const Character& c = game.chars[char_];
  int x = ox + bars->r.x + 10, y = oy + bars->r.y;
  DrawDots(game, canvas, x, y + 10, c.speed, 0);
  DrawDots(game, canvas, x, y + 38, c.accell, 0);
  DrawDots(game, canvas, x, y + 66, c.handling, 0);
  DrawDots(game, canvas, x, y + 94, c.brakes, 0);
}

void CharacterScreen::OnClick(davi::Widget& w) {
  int n = int(game.chars.size());
  // Port: another character takes its own trailer again (the Garage's
  // Caravan button chooses another until the next change).
  auto own_trailer = [&]() {
    if (Player* p = game.Cur()) p->Res(char_).trailer = -1;
  };
  if (w.name == "btnNextChar") {
    SetChar((char_ + 1) % n);
    own_trailer();
  } else if (w.name == "btnPrevChar") {
    SetChar((char_ + n - 1) % n);
    own_trailer();
  } else if (w.name == "btnMain") game.Goto(game.Screen<MainScreen>());
  else if (w.name == "btnBack") game.Back();
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
  else if (w.name == "btnOk") {
    game.Voice(game.chars[char_].chosenwav, 3);
    if (Player* p = game.Cur()) p->current_char = char_;
    game.SavePlayers();
    game.Goto(game.Screen<TrackScreen>());
  }
}

// -------------------------------------------------------------------- Track

TrackScreen::TrackScreen(Game& g) : VrScreen(g, "TfrmTrack") {}

void TrackScreen::OnShow() {
  Player* p = game.Cur();
  SetTrack(p ? std::min(p->Res(p->current_char).tracktodrive, int(game.tracks.size()) - 1) : 0);
  game.app.SetFocus(form.Get("btnOk"));
}

void TrackScreen::OnHide() { game.app.CancelTimers(this); }

void TrackScreen::Cycle() {
  // timCycler (2000 ms): track<t>_0..3 in turn.
  image_ = (image_ + 1) % 4;
  std::string bmp = "track" + std::to_string(game.tracks[track_].index) + "_" + std::to_string(image_);
  form.SetPicture("imgTrack1", game.assets.Bitmap(bmp));
  form.SetVisible("imgTrack2", false);
  game.app.After(2000, [this]() { Cycle(); }, this);
}

void TrackScreen::SetTrack(int i) {
  track_ = i;
  const Track& t = game.tracks[i];
  Player* p = game.Cur();
  CharResult r = p ? p->Res(p->current_char) : CharResult{};
  // Port: Alles vrij opens every track (the progress itself stays).
  int last = r.won || (p && p->unlock_all) ? int(game.tracks.size()) - 1 : r.tracktodrive;
  form.SetEnabled("btnPrevTrack", i > 0);
  form.SetEnabled("btnNextTrack", i < last);
  int q = p && p->difficulty < int(t.qualify.size()) ? t.qualify[p->difficulty] : 0;
  std::string ord = game.Ordinal(q + 1);
  form.SetCaption("lblName", t.name);
  form.SetCaption("lblTitle", t.name);
  form.SetCaption("lblType", t.type);
  form.SetCaption("lblLength", Fmt(game.Str(14), ord));
  form.SetCaption("lblTime", game.FormatTime(game.records[i].time_ms));
  form.SetCaption("lblRecName", game.records[i].name);
  form.SetCaption("lblTextBlock", Fmt(t.story, ord));
  game.app.CancelTimers(this);
  image_ = -1;
  Cycle();
  // tmrSound: the track's intro voice one second later.
  Game& g = game;
  std::string wav = t.introwav;
  game.app.After(1000, [&g, wav]() { g.Voice(wav); }, this);
}

void TrackScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnPrevTrack" && track_ > 0) SetTrack(track_ - 1);
  else if (w.name == "btnNextTrack") SetTrack(track_ + 1);
  else if (w.name == "btnMain") game.Goto(game.Screen<MainScreen>());
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
  else if (w.name == "btnOk") {
    if (Player* p = game.Cur()) p->current_track = track_;
    game.SavePlayers();
    game.Goto(game.Screen<GarageScreen>());
  }
}

// ------------------------------------------------------------------- Garage

GarageScreen::GarageScreen(Game& g) : VrScreen(g, "TfrmGarage") {}

CharResult& GarageScreen::Res() {
  static CharResult none;
  Player* p = game.Cur();
  return p ? p->Res(p->current_char) : none;
}

// 0x480dd8: a tune-up only when not bought yet, repair with damage, turbo
// oil below 100.
void GarageScreen::UpdateButtons() {
  CharResult& r = Res();
  form.SetEnabled("btnTUMotor", !r.tuneup[0]);
  form.SetEnabled("btnTUTires", !r.tuneup[1]);
  form.SetEnabled("btnTUABS", !r.tuneup[2]);
  form.SetEnabled("btnTUGears", !r.tuneup[3]);
  form.SetEnabled("btnRepair", r.damage > 0);
  form.SetEnabled("btnOil", r.turbo < 100);
}

// 0x4813b8: Sunnies, and the selected item's name, text and price.
void GarageScreen::ShowItem() {
  form.SetCaption("lblPoints", std::to_string(Res().money));
  const char* g = "GARAGE";
  if (item_ == 1 && tuneup_ < int(game.tuneups.size())) {
    const TuneUp& t = game.tuneups[tuneup_];
    form.SetCaption("lblTitle", t.name);
    form.SetCaption("lblTextBlock", t.text);
    form.SetCaption("lblPrice", Fmt(game.Str(13), std::to_string(t.price)));
  } else if (item_ == 2) {
    form.SetCaption("lblTitle", game.Str(35));
    form.SetCaption("lblTextBlock", game.Str(20));
    form.SetCaption("lblPrice", Fmt(game.Str(13), std::to_string(game.stat.GetInt(g, "RepairCost", 15))));
  } else if (item_ == 3) {
    form.SetCaption("lblTitle", game.Str(34));
    form.SetCaption("lblTextBlock", game.Str(19));
    form.SetCaption("lblPrice", Fmt(game.Str(13), std::to_string(game.stat.GetInt(g, "TurboOilCost", 20))));
  }
}

void GarageScreen::ShowTrailer() {
  Player* p = game.Cur();
  if (p) ShowTrailerChoice(game, form, p->current_char, Res().trailer);
}

void GarageScreen::OnShow() {
  UpdateButtons();
  ShowItem();
  ShowTrailer();
  // Jacques (0x4817a4): [WAVS] 0 intro, 1 heavy damage, 2 few Sunnies,
  // then 4 (bad) or 3 (good) depending on the last race.
  CharResult& r = Res();
  auto wav = [&](int i) { return game.stat.Get("WAVS", std::to_string(i)); };
  if (game.prevstate == 0) game.Voice(wav(0));
  else if (r.damage > game.stat.GetInt("GARAGE", "CriticalDamage", 70)) game.Voice(wav(1));
  else if (r.money < game.stat.GetInt("GARAGE", "LowPoints", 0)) game.Voice(wav(2));
  else if (!(game.rrflags & kQualify)) game.Voice(wav(4), 4);
  else game.Voice(wav(3), 4);
  game.app.SetFocus(form.Get("btnOk"));
}

// imgDamageOil (0x4816d0): turbo as green dots at (2,2), damage as red dots
// at (2,30), one per 10 percent.
void GarageScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* img = form.Get("imgDamageOil");
  if (!img) return;
  CharResult& r = Res();
  // Delphi Round: halves go to the even number.
  int d = int(std::nearbyint(r.damage / 10.0)), t = int(std::nearbyint(r.turbo / 10.0));
  DrawDots(game, canvas, ox + img->r.x + 2, oy + img->r.y + 30, d, -d);
  DrawDots(game, canvas, ox + img->r.x + 2, oy + img->r.y + 2, 0, t);
}

// btnBuyTuneUpClick (0x480ea4).
void GarageScreen::Buy() {
  CharResult& r = Res();
  auto broke = [&]() { Message(game, game.Str(5)); };
  if (item_ == 1) {
    if (tuneup_ >= int(game.tuneups.size()) || tuneup_ >= 4 || r.tuneup[tuneup_]) return;
    int price = game.tuneups[tuneup_].price;
    if (r.money < price) return broke();
    r.tuneup[tuneup_] = true;
    r.money -= price;
  } else if (item_ == 2) {
    int cost = game.stat.GetInt("GARAGE", "RepairCost", 15);
    if (r.money < cost) return broke();
    if (r.damage <= 0) return;
    r.money -= cost;
    r.damage = std::max(0, r.damage - 10);
  } else if (item_ == 3) {
    int cost = game.stat.GetInt("GARAGE", "TurboOilCost", 20);
    if (r.money < cost) return broke();
    if (r.turbo >= 100) return;
    r.turbo += 10;
    r.money -= cost;
  } else {
    return;
  }
  game.Sound("Buy");
  game.SavePlayers();
  ShowItem();
  UpdateButtons();
}

void GarageScreen::OnClick(davi::Widget& w) {
  auto pick = [&](int t) {
    item_ = 1;
    tuneup_ = t;
    ShowItem();
  };
  if (w.name == "btnTUMotor") pick(0);
  else if (w.name == "btnTUTires") pick(1);
  else if (w.name == "btnTUABS") pick(2);
  else if (w.name == "btnTUGears") pick(3);
  else if (w.name == "btnRepair") item_ = 2, ShowItem();
  else if (w.name == "btnOil") item_ = 3, ShowItem();
  else if (w.name == "btnBuyTuneUp") Buy();
  else if (davi::Lower(w.name) == "btntrailer") {
    // Port: the trailer for this race (kept per character).
    Player* p = game.Cur();
    if (!p) return;
    int n = int(game.trailers.size());
    Res().trailer = (ChosenTrailer(game, p->current_char, Res().trailer) + 1) % n;
    game.SavePlayers();
    ShowTrailer();
  }
  else if (w.name == "btnCancel") game.Back();
  else if (w.name == "btnMain") game.Goto(game.Screen<MainScreen>());
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
  else if (w.name == "btnOk") {
    Player* p = game.Cur();
    if (!p) return;
    game.mode = 2;
    Game& g = game;
    StartRace(game, p->current_char, p->current_track, [&g]() {
      // 0x48eae0: results when finished, else back to the track screen.
      if (g.last.finished) g.Goto(g.Screen<ResultsScreen>());
      else g.Goto(g.Screen<TrackScreen>());
    });
  }
}

// --------------------------------------------------------------- TrackIntro

TrackIntroScreen::TrackIntroScreen(Game& g) : VrScreen(g, "TfrmTrackIntro") {}

// "NextTrck", then the track screen after 3 s (timClose) or a click.
void TrackIntroScreen::OnShow() {
  game.Voice("NextTrck");
  Game& g = game;
  game.app.After(3000, [&g]() { g.Goto(g.Screen<TrackScreen>()); }, this);
}

void TrackIntroScreen::OnHide() { game.app.CancelTimers(this); }

void TrackIntroScreen::OnClick(davi::Widget& w) {
  if (w.name == "imgBackGround") game.Goto(game.Screen<TrackScreen>());
}

}  // namespace vr
