// TfrmQuickRace (QUICKRAC.bmp): character and track selection.
#include "vracer/screens.h"

namespace vr {

// Data.DrawStatusBar (0x4677fc): `value` dots of menu.ini [BITMAPS] 0,
// then -delta of bitmap 1 (lost) or +delta of bitmap 2 (gained), 13 px apart.
void DrawDots(Game& g, SDL_Surface* canvas, int x, int y, int value, int delta) {
  int blue = delta < 0 ? value + delta : value;
  int red = delta < 0 ? -delta : 0;
  int green = delta > 0 ? delta : 0;
  int i = 0;
  auto put = [&](int bmp, int n) {
    SDL_Surface* s = g.assets.Bitmap(g.menu.Get("BITMAPS", std::to_string(bmp)));
    if (!s) return;
    Uint32 key = ((Uint32*)((Uint8*)s->pixels + (s->h - 1) * s->pitch))[0];
    SDL_SetSurfaceColorKey(s, true, key);
    for (int k = 0; k < n; k++, i++) {
      SDL_Rect d{x + i * 13, y, s->w, s->h};
      SDL_BlitSurface(s, nullptr, canvas, &d);
    }
  };
  put(0, blue);
  put(1, red);
  put(2, green);
}

QuickRaceScreen::QuickRaceScreen(Game& g) : VrScreen(g, "TfrmQuickRace") {}

// 0x47fb20: current track; the bonus entry if unlocked, else the last
// character used.
void QuickRaceScreen::OnShow() {
  Player* p = game.Cur();
  form.SetCaption("btnHelp", game.Str(18));
  SetTrack(p ? std::min(p->qr_track, int(game.tracks.size()) - 1) : 0);
  SetChar(game.BonusUnlocked() && !(p && p->unlock_all) ? int(game.chars.size())
          : p ? std::min(p->qr_char, int(game.chars.size())) : 0);
  if (p) {
    SetCar(p->qr_car);
    SetTrailer(p->qr_trailer);
  }
  game.app.SetFocus(form.Get("btnOk"));
  // After-show callback (0x480360): the "Grande Winnaar" popup once all
  // characters have won the Vakantie Race (RR_WINALL of the last race).
  if (game.mode == 2 && (game.rrflags & kWinAll)) game.app.ShowModal(game.Screen<Ferrari2Popup>());
  game.mode = 1;
}

// 0x4805ec SetCharIndex.
void QuickRaceScreen::SetChar(int i) {
  char_ = i;
  bool bonus = i >= int(game.chars.size());
  bool locked = bonus && !game.BonusUnlocked();
  form.SetVisible("lblLocked", locked);
  form.SetEnabled("btnOk", !locked);
  for (const char* b : {"btnPrevCar", "btnNextCar", "btnTrailer"}) form.SetEnabled(b, !locked);
  SetCar(-1);
  SetTrailer(-1);
}

// Port: the car he drives; the picture is his own (with the character) or
// the other car alone, the bars show the car.
void QuickRaceScreen::SetCar(int car) {
  int slot = SlotCar(game, char_);
  if (car == slot || !game.CarAvailable(car)) car = -1;
  car_ = car;
  bool bonus = char_ >= int(game.chars.size());
  davi::Widget* img = form.Get("imgChar");
  if (car_ < 0) {
    form.SetPicture("imgChar", game.assets.Bitmap(bonus ? "qr_pre" : game.chars[char_].qrbmp));
  } else if (img) {
    form.SetPicture("imgChar", game.CarPicture(car_, 211, 161));
  }
  SetTrailer(trailer_);
}

void QuickRaceScreen::SetTrailer(int trailer) {
  trailer_ = trailer < int(game.trailers.size()) ? trailer : -1;
  ShowTrailerChoice(game, form, char_, trailer_);
}

// 0x47feb4 SetCurrentTrackIndex.
void QuickRaceScreen::SetTrack(int i) {
  track_ = i;
  const Track& t = game.tracks[i];
  Player* p = game.Cur();
  int diff = p ? p->difficulty : 1;
  form.SetCaption("lblTrackName", t.name);
  std::string bmp = game.stat.Get("TRACK" + std::to_string(t.index), "QRBITMAP", "qr_trc" + std::to_string(t.index));
  form.SetPicture("imgTrack", game.assets.Bitmap(bmp));
  int q = diff < int(t.qualify.size()) ? t.qualify[diff] : 0;
  const Record& r = game.records[i];
  form.SetCaption("lblTime", game.FormatTime(r.time_ms));
  form.SetCaption("lblRecName", r.name);
  form.SetCaption("lblLength", Fmt(game.Str(14), game.Ordinal(q + 1)));
  form.SetCaption("lblType", t.type);
}

void QuickRaceScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* bars = form.Get("imgBars");
  if (!bars) return;
  int x = ox + bars->r.x + 5, y = oy + bars->r.y;
  bool bonus = char_ >= int(game.chars.size());
  const Character* c = bonus ? nullptr : &game.chars[char_];
  int speed = c ? c->speed : 5, accell = c ? c->accell : 5, handling = c ? c->handling : 5,
      brakes = c ? c->brakes : 5;
  // Port: another car shows that car's bars.
  if (const Car* car = car_ >= 0 ? game.CarInfo(car_) : nullptr) {
    speed = car->speed;
    accell = car->accell;
    handling = car->handling;
    brakes = car->brakes;
  }
  // Rows: Snelheid, Acceleratie, Wegligging, Remkracht (0x480448).
  DrawDots(game, canvas, x, y + 5, speed, 0);
  DrawDots(game, canvas, x, y + 33, accell, 0);
  DrawDots(game, canvas, x, y + 61, handling, 0);
  DrawDots(game, canvas, x, y + 89, brakes, 0);
}

void QuickRaceScreen::OnClick(davi::Widget& w) {
  int nc = int(game.chars.size()) + 1, nt = int(game.tracks.size());
  if (w.name == "btnNextChar") SetChar((char_ + 1) % nc);
  else if (w.name == "btnPrevChar") SetChar((char_ + nc - 1) % nc);
  else if (w.name == "btnNextTrack") SetTrack((track_ + 1) % nt);
  else if (davi::Lower(w.name) == "btnnextcar" || davi::Lower(w.name) == "btnprevcar") {
    int from = car_ < 0 ? SlotCar(game, char_) : car_;
    SetCar(game.NextCar(from, davi::Lower(w.name) == "btnnextcar" ? 1 : -1));
  } else if (davi::Lower(w.name) == "btntrailer") {
    int n = int(game.trailers.size());
    SetTrailer((ChosenTrailer(game, char_, trailer_) + 1) % n);
  }
  else if (w.name == "btnPrevTrack") SetTrack((track_ + nt - 1) % nt);
  else if (w.name == "btnMain") game.Goto(game.Screen<MainScreen>());
  else if (w.name == "btnCancel") game.Goto(game.Screen<DifficultyScreen>());
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
  else if (w.name == "btnOk") {
    if (Player* p = game.Cur()) {
      p->qr_char = char_;
      p->qr_track = track_;
      p->qr_car = car_;
      p->qr_trailer = trailer_;
      game.SavePlayers();
    }
    Game& g = game;
    StartRace(game, char_, track_, [&g]() {
      // 0x48e9d0: results when finished, else back to the Quick Race screen.
      if (g.last.finished) g.Goto(g.Screen<ResultsScreen>());
      else g.Goto(g.Screen<QuickRaceScreen>());
    });
  }
}

}  // namespace vr
