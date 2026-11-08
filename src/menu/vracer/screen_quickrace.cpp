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

bool QuickRaceScreen::BonusUnlocked() {
  // FUN_0046c808: every character has won the Vakantie Race.
  Player* p = game.Cur();
  if (!p) return false;
  for (auto& c : game.chars)
    if (!p->Res(c.index).won) return false;
  return true;
}

// 0x47fb20: current track; the bonus entry if unlocked, else the last
// character used.
void QuickRaceScreen::OnShow() {
  Player* p = game.Cur();
  form.SetCaption("btnHelp", game.Str(18));
  SetTrack(p ? std::min(p->qr_track, int(game.tracks.size()) - 1) : 0);
  SetChar(BonusUnlocked() ? int(game.chars.size()) : p ? std::min(p->qr_char, int(game.chars.size())) : 0);
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
  bool locked = bonus && !BonusUnlocked();
  form.SetVisible("lblLocked", locked);
  form.SetEnabled("btnOk", !locked);
  form.SetPicture("imgChar", game.assets.Bitmap(bonus ? "qr_pre" : game.chars[i].qrbmp));
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
  // Rows: Snelheid, Acceleratie, Wegligging, Remkracht (0x480448).
  DrawDots(game, canvas, x, y + 5, c ? c->speed : 5, 0);
  DrawDots(game, canvas, x, y + 33, c ? c->accell : 5, 0);
  DrawDots(game, canvas, x, y + 61, c ? c->handling : 5, 0);
  DrawDots(game, canvas, x, y + 89, c ? c->brakes : 5, 0);
}

void QuickRaceScreen::OnClick(davi::Widget& w) {
  int nc = int(game.chars.size()) + 1, nt = int(game.tracks.size());
  if (w.name == "btnNextChar") SetChar((char_ + 1) % nc);
  else if (w.name == "btnPrevChar") SetChar((char_ + nc - 1) % nc);
  else if (w.name == "btnNextTrack") SetTrack((track_ + 1) % nt);
  else if (w.name == "btnPrevTrack") SetTrack((track_ + nt - 1) % nt);
  else if (w.name == "btnMain") game.Goto(game.Screen<MainScreen>());
  else if (w.name == "btnCancel") game.Goto(game.Screen<DifficultyScreen>());
  else if (w.name == "btnHelp") game.Voice("jac_hlp", 3);
  else if (w.name == "btnOk") {
    if (Player* p = game.Cur()) {
      p->qr_char = char_;
      p->qr_track = track_;
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
