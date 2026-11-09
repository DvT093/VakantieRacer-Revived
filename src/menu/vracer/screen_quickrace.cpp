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
      davi::Blit(canvas, s, nullptr, d);
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
  int nchars = int(game.chars.size());
  std::vector<int> e = Entries();
  int k = 0;
  if (game.BonusUnlocked() && !(p && p->unlock_all)) {
    k = nchars;
  } else if (p) {
    k = std::min(p->qr_char, nchars);
    if (k == nchars)  // the bonus entry: the bonus car chosen last time
      for (size_t i = nchars; i < e.size(); i++)
        if (e[i] == p->qr_car) k = int(i);
  }
  SetEntry(k);
  if (p) SetTrailer(p->qr_trailer);
  game.app.SetFocus(form.Get("btnOk"));
  // After-show callback (0x480360): the "Grande Winnaar" popup once all
  // characters have won the Vakantie Race (RR_WINALL of the last race).
  if (game.mode == 2 && (game.rrflags & kWinAll)) game.app.ShowModal(game.Screen<Ferrari2Popup>());
  game.mode = 1;
}

// The characters' cars, then the Ferrari (always: the original shows it
// locked) and, port, the other bonus cars once they are open.
std::vector<int> QuickRaceScreen::Entries() {
  std::vector<int> e;
  for (auto& c : game.chars) e.push_back(c.indexingame);
  e.push_back(SlotCar(game, int(game.chars.size())));
  for (size_t car = 0; car < game.cars.size(); car++) {
    const Car* info = game.CarInfo(int(car));
    if (info && info->bonus && int(car) != e.back() && game.CarAvailable(int(car))) e.push_back(int(car));
  }
  return e;
}

// 0x4805ec SetCharIndex: the picture (character with his car, the presenter
// with the Ferrari, or the bonus car alone), the lock, and the car's own
// trailer.
void QuickRaceScreen::SetEntry(int k) {
  std::vector<int> e = Entries();
  int nchars = int(game.chars.size());
  k = std::max(0, std::min(k, int(e.size()) - 1));
  bool bonus = k >= nchars;
  char_ = bonus ? nchars : k;
  car_ = bonus ? e[k] : -1;
  bool locked = bonus && !game.BonusUnlocked();
  form.SetVisible("lblLocked", locked);
  form.SetEnabled("btnOk", !locked);
  locked_ = locked;
  if (!bonus) form.SetPicture("imgChar", game.assets.Bitmap(game.chars[k].qrbmp));
  else if (car_ == SlotCar(game, nchars)) form.SetPicture("imgChar", game.assets.Bitmap("qr_pre"));
  else form.SetPicture("imgChar", game.CarPicture(car_, 211, 161));
  SetTrailer(-1);
}

void QuickRaceScreen::Step(int dir) {
  std::vector<int> e = Entries();
  int n = int(e.size()), k = char_;
  for (int i = int(game.chars.size()); i < n; i++)
    if (char_ >= int(game.chars.size()) && e[i] == car_) k = i;
  SetEntry(((k + dir) % n + n) % n);
}

void QuickRaceScreen::SetTrailer(int trailer) {
  trailer_ = trailer < int(game.trailers.size()) ? trailer : -1;
  ShowTrailerChoice(game, form, char_, trailer_, !locked_);
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
  // The bonus entry shows the bars of its car ([carN] in static.ini; the
  // Ferrari has all 5, as 0x480448).
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
  int nt = int(game.tracks.size());
  std::string name = davi::Lower(w.name);
  // Port: a car belongs to its character, so the character row picks the
  // car too; the caravan thumbnails below the picture pick the trailer
  // until the next car change.
  if (name == "btnnextchar") Step(1);
  else if (name == "btnprevchar") Step(-1);
  else if (w.name == "btnNextTrack") SetTrack((track_ + 1) % nt);
  else if (int t = TrailerThumb(w); t >= 0) SetTrailer(t);
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
