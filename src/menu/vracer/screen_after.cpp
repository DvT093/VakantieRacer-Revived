// Vakantie Race, after the results: TfrmHost, TfrmTrackWinner,
// TfrmWheelOfFortune, TfrmWinRace and the TfrmFerrari2 popup.
#include <cmath>
#include <random>

#include "vracer/screens.h"

namespace vr {

namespace {

int Random(int n) {
  static std::mt19937 r{std::random_device{}()};
  return n > 0 ? std::uniform_int_distribution<int>(0, n - 1)(r) : 0;
}

const Character& CurChar(Game& g) {
  Player* p = g.Cur();
  int c = p ? p->current_char : 0;
  return g.chars[std::max(0, std::min(c, int(g.chars.size()) - 1))];
}

const Track& CurTrack(Game& g) {
  Player* p = g.Cur();
  int t = p ? p->current_track : 0;
  return g.tracks[std::max(0, std::min(t, int(g.tracks.size()) - 1))];
}

}  // namespace

// --------------------------------------------------------------------- Host

HostScreen::HostScreen(Game& g) : VrScreen(g, "TfrmHost") {}

// The presenter on the player's place: qualified or out.
void HostScreen::OnShow() {
  const Character& c = CurChar(game);
  const Track& t = CurTrack(game);
  bool q = game.rrflags & kQualify;
  form.SetPicture("imgPosition", game.assets.Bitmap("res_pos" + std::to_string(game.playerpos + 1)));
  form.SetCaption("lblTrack", t.name);
  form.SetPicture("imgChar", game.assets.Bitmap(q ? c.hostqualbmp : c.hostlosebmp));
  form.SetCaption("lblTextBlock", q ? t.qualifytext : t.losetext);
  form.SetCaption("lblPoints", std::to_string(game.Cur() ? game.Cur()->Res(c.index).money : 0));
  game.Voice((q ? "prf2tr" : "prf3tr") + std::to_string(t.index));
  game.app.SetFocus(form.Get("btnOk"));
}

void HostScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") game.Goto(game.Screen<TrackWinnerScreen>());
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
}

// -------------------------------------------------------------- TrackWinner

TrackWinnerScreen::TrackWinnerScreen(Game& g) : VrScreen(g, "TfrmTrackWinner") {}

// The computer racer that won the stage.
void TrackWinnerScreen::OnShow() {
  const Track& t = CurTrack(game);
  int w = game.ranking.empty() ? -1 : game.ranking[0].character;
  form.SetCaption("lblTrackName", t.name);
  if (w >= 0 && w < int(game.chars.size())) {
    const Character& c = game.chars[w];
    form.SetCaption("lblTitle", Fmt(game.Str(27), c.name));
    form.SetCaption("lblTextBlock", c.wintext);
    form.SetPicture("imgCharacter", game.assets.Bitmap(c.hostwinbmp));
    game.Voice("rac" + std::to_string(w) + "f", 5);
  }
  game.app.SetFocus(form.Get("btnOk"));
}

void TrackWinnerScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") {
    if (game.rrflags & kQualify) game.Goto(game.Screen<TrackIntroScreen>());
    else game.Goto(game.Screen<TrackScreen>());
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

// -------------------------------------------------------- Wheel of fortune

// The 20 lights around the wheel (table at 0x4950a0); two lights per prize.
static const SDL_Point kLights[20] = {
    {333, 147}, {365, 164}, {389, 189}, {404, 220}, {409, 255}, {404, 289}, {389, 318},
    {364, 343}, {334, 359}, {299, 365}, {265, 359}, {233, 343}, {209, 321}, {193, 288},
    {187, 255}, {192, 220}, {208, 188}, {231, 164}, {262, 148}, {298, 142}};

WheelScreen::WheelScreen(Game& g) : VrScreen(g, "TfrmWheelOfFortune") {}

void WheelScreen::Buttons() {
  form.SetEnabled("btnStartWheel", state_ == 1);
  form.SetEnabled("btnStopWheel", state_ == 2);
  form.SetEnabled("btnOk", state_ == 3);
}

// State 1: a light jumps around every 250 ms until Start.
void WheelScreen::OnShow() {
  const Character& c = CurChar(game);
  const Track& t = CurTrack(game);
  state_ = 1;
  form.SetPicture("imgCharacter", game.assets.Bitmap(c.wofbmp));
  form.SetPicture("imgPrize", nullptr);
  form.SetCaption("lblTitle", Fmt(game.Str(26), c.name));
  form.SetCaption("lblTextBlock", t.wintext);
  form.SetCaption("lblTrack", t.name);
  form.SetCaption("lblPoints", std::to_string(game.Cur() ? game.Cur()->Res(c.index).money : 0));
  game.Voice("prf1tr" + std::to_string(t.index));
  Buttons();
  game.app.SetFocus(form.Get("btnStartWheel"));
  auto tick = std::make_shared<std::function<void()>>();
  *tick = [this, tick]() {
    if (state_ != 1) return;
    light_ = Random(20);
    game.app.After(250, *tick, this);
  };
  (*tick)();
}

void WheelScreen::OnHide() { game.app.CancelTimers(this); }

// State 2: one light further per tick with the wheel tick sound; on even
// lights the interval grows by `factor` (1.15, or 1.5 after Stop) until it
// passes one second.
void WheelScreen::Step() {
  if (state_ != 2) return;
  light_ = (light_ + 1) % 20;
  game.Sound("Wheel");
  if (light_ % 2 == 0) {
    interval_ = std::round(interval_ * factor_);
    if (interval_ > 1000) {
      state_ = 3;
      Award();
      Buttons();
      game.app.SetFocus(form.Get("btnOk"));
      return;
    }
  }
  game.app.After(int(interval_), [this]() { Step(); }, this);
}

// 0x48c43c: [wofprizes] light/2. Sunnies (0 = nothing, -1 = a full turbo
// tank, which also takes one Sunny as in the original) or a tune-up.
void WheelScreen::Award() {
  int idx = light_ / 2;
  if (idx >= int(game.prizes.size())) return;
  const Prize& pz = game.prizes[idx];
  const Character& c = CurChar(game);
  Player* p = game.Cur();
  if (!p) return;
  CharResult& r = p->Res(c.index);
  if (SDL_Surface* s = game.assets.Bitmap(pz.imagefile)) form.SetPicture("imgPrize", s);
  if (!pz.istuneup) {
    if (pz.value == 0) game.Voice(c.rndwinnothingwav, 4);
    else if (pz.value == -1) game.Voice(c.rndwinprizewav, 4), r.turbo = 100;
    else game.Voice(c.rndwinprizewav, 3);
    r.money += pz.value;
  } else {
    if (pz.value >= 0 && pz.value < 4) r.tuneup[pz.value] = true;
    game.Voice(c.rndwinprizewav, 3);
  }
  form.SetCaption("lblPoints", std::to_string(r.money));
  game.SavePlayers();
}

void WheelScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* on = form.Get("LightOn");
  davi::Widget* off = form.Get("LightOff");
  for (int i = 0; i < 20; i++) {
    davi::Widget* w = i == light_ ? on : off;
    if (!w || !w->picture) continue;
    SDL_Rect d{ox + kLights[i].x, oy + kLights[i].y, w->picture->w, w->picture->h};
    SDL_BlitSurface(w->picture, nullptr, canvas, &d);
  }
}

void WheelScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnStartWheel" && state_ == 1) {
    game.Sound("startrad");
    state_ = 2;
    light_ = Random(20);
    interval_ = 50;
    factor_ = 1.15;
    Buttons();
    game.app.SetFocus(form.Get("btnStopWheel"));
    game.app.CancelTimers(this);
    game.app.After(int(interval_), [this]() { Step(); }, this);
  } else if (w.name == "btnStopWheel" && state_ == 2) {
    game.Sound("stoprad");
    factor_ = 1.5;
    form.SetEnabled("btnStopWheel", false);
  } else if (w.name == "btnOk" && state_ == 3) {
    game.Goto(game.Screen<TrackIntroScreen>());
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

// ------------------------------------------------------------------ WinRace

WinRaceScreen::WinRaceScreen(Game& g) : VrScreen(g, "TfrmWinRace") {}

// Won the final: the winner's text and outro voice. The original also plays
// the character's AVI (static.ini video=) in pnlMovie (224,144 308x233).
void WinRaceScreen::OnShow() {
  const Character& c = CurChar(game);
  form.SetCaption("lblTitle", Fmt(game.Str(28), c.name));
  form.SetCaption("lblTextBlock", c.winracetext);
  game.Voice(c.outrowav);
  game.app.SetFocus(form.Get("btnOk"));
  std::string avi = game.assets.Find(c.video);
  Player* p = game.Cur();
  if (!avi.empty()) movie_.Open(avi, &game.app.audio, (p ? p->fx_volume : 75) / 100.0f);
}

void WinRaceScreen::OnHide() { movie_.Close(); }

// medWinnerNotify: the movie loops until OK.
void WinRaceScreen::Tick(double) {
  if (movie_.IsOpen() && !movie_.Update()) movie_.Restart();
}

void WinRaceScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* pnl = form.Get("pnlMovie");
  if (!pnl || !movie_.Frame()) return;
  SDL_Rect r{ox + pnl->r.x, oy + pnl->r.y, pnl->r.w, pnl->r.h};
  SDL_BlitSurfaceScaled(movie_.Frame(), nullptr, canvas, &r, SDL_SCALEMODE_LINEAR);
}

void WinRaceScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") {
    if (game.rrflags & kWinAll) game.Goto(game.Screen<QuickRaceScreen>());
    else game.Goto(game.Screen<CharacterScreen>());
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

// ----------------------------------------------------------------- Ferrari2

Ferrari2Popup::Ferrari2Popup(Game& g) : VrScreen(g, "TfrmFerrari2") {
  form.background = g.assets.Bitmap("pop_gwin");
  form.SetPicture("imgBackGround", form.background);
}

void Ferrari2Popup::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") Close(1);
}

}  // namespace vr
