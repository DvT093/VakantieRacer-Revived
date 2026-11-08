// TfrmPlayer (PLAYER.bmp) and TfrmMenuDialog.
#include "vracer/screens.h"

namespace vr {

// ------------------------------------------------------------------- Player

PlayerScreen::PlayerScreen(Game& g) : VrScreen(g, "TfrmPlayer") {}

// FormShow (0x484240): keep a copy to restore on Annuleer (the original
// copied save*.rcr to bsave*.rcr), then UpdateScreen.
void PlayerScreen::OnShow() {
  backup_ = game.players;
  backup_current_ = game.current_player;
  Update();
  game.app.SetFocus(form.Get("btnOk"));
}

// UpdateScreen (0x484c2c): btn0 = current player with the portrait of the
// character last played (def<char>), btn1..4 = the other players, empty
// slots "Nieuwe speler" (MENUSTRINGS 17) with defEmpty.
void PlayerScreen::Update() {
  auto def = [&](int c) { return game.app.skin.Def(form.cls, "def" + std::to_string(c)); };
  Player* cur = game.Cur();
  davi::Widget& b0 = form.W("btn0");
  b0.def = def(cur ? std::max(0, std::min(cur->current_char, 4)) : 0);
  b0.caption = cur ? cur->name : "";
  others_.clear();
  for (int i = 0; i < int(game.players.size()); i++)
    if (i != game.current_player) others_.push_back(i);
  for (int k = 0; k < 4; k++) {
    davi::Widget& b = form.W("btn" + std::to_string(k + 1));
    b.visible = true;
    if (k < int(others_.size())) {
      const Player& p = game.players[others_[k]];
      b.def = def(std::max(0, std::min(p.current_char, 4)));
      b.caption = p.name;
    } else {
      b.def = game.app.skin.Def(form.cls, "defEmpty");
      b.caption = game.Str(17);
    }
  }
  form.SetEnabled("btnDeletePlayer", !others_.empty());
}

void PlayerScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") {
    game.SavePlayers();
    game.Back();
  } else if (w.name == "btnCancel") {
    game.players = backup_;
    game.current_player = backup_current_;
    game.SavePlayers();
    game.Back();
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  } else if (w.name.size() == 4 && w.name.rfind("btn", 0) == 0 && w.name[3] >= '1' && w.name[3] <= '4') {
    int k = w.name[3] - '1';
    if (k < int(others_.size())) {
      game.current_player = others_[k];  // LoadPlayer
      game.SavePlayers();
      game.StartMusic();
      Update();
    } else {
      // A new player keeps the current player's settings (0x484368).
      Player settings = game.Cur() ? *game.Cur() : Player{};
      AskForNewPlayer(game, [this, settings]() {
        Player& p = *game.Cur();
        std::string name = p.name;
        auto results = p.results;
        p = settings;
        p.name = name;
        p.results = results;
        p.current_char = 4;
        p.current_track = 0;
        game.SavePlayers();
        Update();
      });
    }
  } else if (w.name == "btnDeletePlayer") {
    // MENUSTRINGS 4 "Wil je speler %s uit de spelerlijst verwijderen?", Ja/Nee.
    Player* p = game.Cur();
    if (!p) return;
    game.Screen<MenuDialog>()->Open(Fmt(game.Str(4), p->name), "", 4 | 8, [this](int r) {
      if (r != 6 || others_.empty()) return;
      int del = game.current_player, next = others_[0];
      game.players.erase(game.players.begin() + del);
      game.current_player = next > del ? next - 1 : next;
      game.SavePlayers();
      Update();
    });
  }
}

// --------------------------------------------------------------- MenuDialog

MenuDialog::MenuDialog(Game& g) : VrScreen(g, "TfrmMenuDialog") {}

// 0x473210: text and title, the chosen buttons centered in the order
// OK, Ja, Nee, Annuleer; wavRequest when it opens, wavReqClose when it closes.
void MenuDialog::Open(const std::string& text, const std::string& title, int buttons, std::function<void(int)> done) {
  form.SetCaption("lblDialogText", text);
  form.SetCaption("lblTitle", title);
  const char* names[4] = {"btnOk", "btnYes", "btnNo", "btnCancel"};
  const int bits[4] = {1, 4, 8, 2};
  int total = 0;
  for (int i = 0; i < 4; i++) {
    davi::Widget& b = form.W(names[i]);
    b.visible = buttons & bits[i];
    if (b.visible) total += b.r.w;
  }
  int x = (form.w - total) / 2;
  for (int i = 0; i < 4; i++) {
    davi::Widget& b = form.W(names[i]);
    if (!b.visible) continue;
    b.r.x = x;
    x += b.r.w - 1;
  }
  game.Sound("__wavRequest");
  Game& g = game;
  game.app.ShowModal(this, [&g, done](int r) {
    g.Sound("__wavReqClose");
    if (done) done(r);
  });
}

void MenuDialog::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") Close(1);
  else if (w.name == "btnCancel") Close(2);
  else if (w.name == "btnYes") Close(6);
  else if (w.name == "btnNo") Close(7);
}

void Message(Game& g, const std::string& text, std::function<void(int)> done) {
  g.Screen<MenuDialog>()->Open(text, "", 1, std::move(done));
}

}  // namespace vr
