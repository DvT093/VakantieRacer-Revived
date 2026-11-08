// TfrmResults (RESULTS.bmp), the start-info popup (TfrmStartInfo) and the
// name popup (TfrmPlayerName).
#include "vracer/screens.h"

namespace vr {

ResultsScreen::ResultsScreen(Game& g) : VrScreen(g, "TfrmResults") {}

// FormCreate (0x48cb6c) + FormShow (0x48cdd4): the characters by place
// (res_*.bmp, the locked Quick Race car as res_pre), a frame on the
// player's row, "applaus", then the lap record name (0x48cff0).
void ResultsScreen::OnShow() {
  for (int i = 0; i < 5; i++) {
    davi::Widget& img = form.W("img" + std::to_string(i));
    img.picture = nullptr;
    if (i >= int(game.ranking.size())) continue;
    int c = game.ranking[i].character;
    img.picture = game.assets.Bitmap(c >= 0 && c < int(game.chars.size()) ? game.chars[c].resultsbmp : "res_pre");
    img.pic_transparent = true;
    img.key_rgb = 0xFF007B;  // TransparentColor RGB(255, 0, 123)
  }
  davi::Widget& focus = form.W("imgFocus");
  focus.r.y = 214 + 60 * game.playerpos;
  focus.pic_transparent = true;
  focus.key_rgb = 0xFF007F;  // TransparentColor RGB(255, 0, 127)
  game.Voice("applaus");
  game.app.SetFocus(form.Get("btnOk"));

  if (game.rrflags & kBestLap) {
    Player* p = game.Cur();
    auto* pop = game.Screen<PlayerNamePopup>();
    int track = game.mode == 1 ? (p ? p->qr_track : 0) : (p ? p->current_track : 0);
    pop->Setup("pop_rond", "", p ? p->name : "");
    Game& g = game;
    auto ask = std::make_shared<std::function<void()>>();
    *ask = [&g, pop, track, ask]() {
      g.app.ShowModal(pop, [&g, pop, track, ask](int) {
        if (pop->name.empty()) {
          (*ask)();
          return;
        }
        g.records[track].name = pop->name;
        g.SaveRecords();
      });
    };
    (*ask)();
  }
}

// grdResults: one 60 px row per place; the winner's time, then " + " and
// the gap to the winner. The player's row in the highlight font.
void ResultsScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  davi::Widget* grid = form.Get("grdResults");
  if (!grid || game.ranking.empty()) return;
  auto& caps = game.app.skin.caps;
  auto get = [&](const char* k, const char* def) {
    std::string key = std::string("grdResults.") + k;
    return caps.Has("TfrmResults", key) ? caps.Get("TfrmResults", key) : caps.Get("DEFAULT", k, def);
  };
  auto color = [](const std::string& v) {
    long c = std::strtol(v.c_str(), nullptr, 0);
    return SDL_Color{Uint8(c & 0xff), Uint8((c >> 8) & 0xff), Uint8((c >> 16) & 0xff), 255};
  };
  auto px = [](const std::string& pt) { return int(std::lround(std::atoi(pt.c_str()) * 96.0 / 72.0)); };
  int colw = std::atoi(get("ColWidth0", "100").c_str());
  for (size_t i = 0; i < game.ranking.size() && i < 5; i++) {
    std::string t = i == 0 ? game.FormatTime(game.ranking[0].time_ms)
                           : " + " + game.FormatTime(game.ranking[i].time_ms - game.ranking[0].time_ms);
    bool hi = int(i) == game.playerpos;
    davi::TextStyle st{game.app.skin.GetFont(1), px(get(hi ? "HighlightSize" : "FontSize", "12")),
                       color(get(hi ? "HighlightColor" : "FontColor", "0")), davi::HAlign::kLeft,
                       davi::VAlign::kCenter, false};
    SDL_Rect r{ox + grid->r.x, oy + grid->r.y + 60 * int(i), colw, 60};
    davi::DrawText(canvas, r, t, st);
  }
}

// btnOkClick (0x48ca24).
void ResultsScreen::OnClick(davi::Widget& w) {
  if (w.name != "btnOk") return;
  if (game.mode == 1) game.Goto(game.Screen<QuickRaceScreen>());
  else if (game.rrflags & kWinRace) game.Goto(game.Screen<WinRaceScreen>());
  else if (game.rrflags & kFortune) game.Goto(game.Screen<WheelScreen>());
  else game.Goto(game.Screen<HostScreen>());
}

StartInfoPopup::StartInfoPopup(Game& g) : VrScreen(g, "TfrmStartInfo") {
  form.background = g.assets.Bitmap("POP_STRT");
  form.SetPicture("imgBackGround", form.background);
}

// FormShow (0x47f388): the player's keys by their menu.ini [CONTROLKEYS] names.
void StartInfoPopup::OnShow() {
  Player* p = game.Cur();
  auto name = [&](int vk) {
    for (auto& k : game.menu.Keys("CONTROLKEYS")) {
      auto v = davi::Split(game.menu.Get("CONTROLKEYS", k), ',');
      if (v.size() >= 2 && k[0] != '*' && std::atoi(v[0].c_str()) == vk) return v[1];
    }
    return std::string();
  };
  // keys: up down left right turbo handbrake horn
  const char* lbl[7] = {"lblSetGas", "lblSetBrake", "lblSetLeft", "lblSetRight", "lblSetTurbo",
                        "lblSetParkingBrake", "lblSetHorn"};
  for (int k = 0; k < 7; k++) form.SetCaption(lbl[k], p ? name(p->keys[k]) : "");
  form.W("chkStartInfo").checked = p ? p->show_start_info : true;
  game.app.SetFocus(form.Get("btnOk"));
}

void StartInfoPopup::OnClick(davi::Widget& w) {
  if (w.name == "btnOk" || w.name == "btnCancel") {
    if (Player* p = game.Cur()) p->show_start_info = form.W("chkStartInfo").checked;
    game.SavePlayers();
    Close(w.name == "btnOk" ? 1 : 2);
  }
}

PlayerNamePopup::PlayerNamePopup(Game& g) : VrScreen(g, "TfrmPlayerName") {
  default_text_ = form.W("lblEnterName").caption;
}

void PlayerNamePopup::Setup(const std::string& bmp, const std::string& text, const std::string& nm) {
  form.background = game.assets.Bitmap(bmp);
  form.SetPicture("imgBackGround", form.background);
  text_ = text;
  initial_ = nm;
}

// AskForNewPlayer (0x48ded8) / lap record (0x48cff0): no Cancel, OK
// centered, the name preselected.
void PlayerNamePopup::OnShow() {
  form.SetCaption("lblEnterName", text_.empty() ? default_text_ : text_);
  form.SetVisible("btnCancel", false);
  davi::Widget& ok = form.W("btnOk");
  ok.r.x = (form.w - ok.r.w) / 2;
  davi::Widget& ed = form.W("txtPlayerName");
  ed.caption = initial_;
  ed.select_all = true;
  game.app.SetFocus(&ed);
}

void PlayerNamePopup::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") {
    name = davi::Trim(form.W("txtPlayerName").caption);
    Close(1);
  }
}

}  // namespace vr
