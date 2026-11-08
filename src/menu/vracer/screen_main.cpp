// TfrmMainScreen, TfrmDifficulty, TfrmPlayerName and the start-up flow.
#include "vracer/screens.h"

namespace vr {

std::string Fmt(const std::string& f, const std::string& a) {
  std::string s = f;
  size_t p = s.find("%s");
  if (p != std::string::npos) s.replace(p, 2, a);
  return s;
}

// TfrmMain.BackFromIntro (0x48dd44): music, then the current player or
// AskForNewPlayer (0x48ded8), then the main screen.
void AskForNewPlayer(Game& g, std::function<void()> done) {
  auto ask = std::make_shared<std::function<void()>>();
  *ask = [&g, ask, done]() {
    auto* pop = g.Screen<PlayerNamePopup>();
    pop->Setup("POP_SPLR", g.Str(7), g.Str(8));
    g.app.ShowModal(pop, [&g, pop, ask, done](int) {
      if (pop->name.empty()) {  // repeat until a name is entered
        (*ask)();
        return;
      }
      g.NewPlayer(pop->name);
      done();
    });
  };
  (*ask)();
}

// TfrmMain.FormActivate on a fresh start (0x48dc18): the intro when the
// file of menu.ini INTROAVI is there, then BackFromIntro.
void Start(Game& g, bool intro) {
  if (intro) {
    // INTROAVI=DATA\INTRO1.MPG is relative to the CD root.
    std::string cd = g.launcher.install().game_dir;
    cd = cd.substr(0, cd.find_last_of('/'));
    std::string path = davi::FindNoCase(cd, g.menu.Get("GENERAL", "INTROAVI", "DATA\\INTRO1.MPG"));
    auto* scr = g.Screen<IntroScreen>();
    if (!path.empty() && scr->Play(path)) {
      g.Goto(scr);
      return;
    }
  }
  BackFromIntro(g);
}

void BackFromIntro(Game& g) {
  g.StartMusic();
  if (g.Cur()) {
    g.Goto(g.Screen<MainScreen>());
    return;
  }
  AskForNewPlayer(g, [&g]() {
    g.StartMusic();
    g.Goto(g.Screen<MainScreen>());
  });
}

IntroScreen::IntroScreen(Game& g) : VrScreen(g, "TfrmIntro") {
  // TfrmIntro.FormMouseDown: any click skips.
  if (davi::Widget* bg = form.Get("imgBackGround")) bg->onclick = "FormMouseDown";
}

bool IntroScreen::Play(const std::string& path) {
  done_ = false;
  Player* p = game.Cur();
  return movie_.Open(path, &game.app.audio, (p ? p->fx_volume : 75) / 100.0f);
}

void IntroScreen::Done() {
  if (done_) return;
  done_ = true;
  movie_.Close();
  BackFromIntro(game);
}

void IntroScreen::OnClick(davi::Widget&) { Done(); }

bool IntroScreen::OnKey(SDL_Keycode) {
  Done();
  return true;
}

void IntroScreen::Tick(double) {
  if (!done_ && !movie_.Update()) Done();
}

// Full screen (MCI on the whole 800x600 form), black around it.
void IntroScreen::Paint(SDL_Surface* canvas, int ox, int oy) {
  SDL_Rect r{ox, oy, form.w, form.h};
  SDL_FillSurfaceRect(canvas, &r, 0);
  if (SDL_Surface* f = movie_.Frame()) SDL_BlitSurfaceScaled(f, nullptr, canvas, &r, SDL_SCALEMODE_LINEAR);
}

MainScreen::MainScreen(Game& g) : VrScreen(g, "TfrmMainScreen") {}

// 0x4856f8: "Welkom %s!" (MENUSTRINGS 9) and the hint (10).
void MainScreen::OnShow() {
  Player* p = game.Cur();
  form.SetCaption("lblTitle", Fmt(game.Str(9), p ? p->name : ""));
  form.SetCaption("lblTextBlock", game.Str(10));
  form.SetCaption("btnHelp", game.Str(18));
}

void MainScreen::OnClick(davi::Widget& w) {
  if (w.name == "btnVacRace") {
    game.mode = 2;
    game.Goto(game.Screen<DifficultyScreen>());
  } else if (w.name == "btnQuickRace") {
    game.mode = 1;
    game.Goto(game.Screen<DifficultyScreen>());
  } else if (w.name == "btnPlayer") {
    game.Goto(game.Screen<PlayerScreen>());
  } else if (w.name == "btnOptions") {
    game.Goto(game.Screen<OptionsScreen>());
  } else if (w.name == "imgCar") {
    // A small easter egg on the main screen (0x485b94).
    if (game.Cur() && game.Cur()->name == "Frits") {
      game.Screen<MenuDialog>()->Open("Lang leve Michel en Hester!", "Het Vakantie Racer Menu zegt:", 1, nullptr);
    }
  } else if (w.name == "btnExit") {
    game.app.Quit(0);
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

DifficultyScreen::DifficultyScreen(Game& g) : VrScreen(g, "TfrmDifficulty") {}

// 0x483660: hint 11 (Vakantie Race) or 12 (Quick Race); focus on the
// player's current difficulty.
void DifficultyScreen::OnShow() {
  form.SetCaption("lblTextBlock", game.Str(game.mode == 2 ? 11 : 12));
  form.SetCaption("btnHelp", game.Str(18));
  Player* p = game.Cur();
  static const char* btn[] = {"btnEasy", "btnMedium", "btnDifficult"};
  int d = p ? std::max(0, std::min(2, p->difficulty)) : 1;
  game.app.SetFocus(form.Get(btn[d]));
}

void DifficultyScreen::OnClick(davi::Widget& w) {
  int d = w.name == "btnEasy" ? 0 : w.name == "btnMedium" ? 1 : w.name == "btnDifficult" ? 2 : -1;
  if (d >= 0) {
    if (Player* p = game.Cur()) p->difficulty = d;
    game.SavePlayers();
    if (game.mode == 1) game.Goto(game.Screen<QuickRaceScreen>());
    else game.Goto(game.Screen<CharacterScreen>());
  } else if (w.name == "btnMain") {
    game.Goto(game.Screen<MainScreen>());
  } else if (w.name == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

}  // namespace vr
