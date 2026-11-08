// Window, main loop and screen stack of the menu. A Screen is one Delphi
// form plus the flow code behind it; popups are modal screens drawn
// centered on top of the current one.
#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "engine/assets.h"
#include "engine/form.h"

namespace davi {

class App;

class Screen {
 public:
  Screen(App& app, const std::string& form_cls);
  virtual ~Screen() = default;
  Form form;
  App& app;
  bool built = false;

  virtual void OnShow() {}
  virtual void OnHide() {}
  // A button (or clickable image/label) was clicked.
  virtual void OnClick(Widget& w) { (void)w; }
  // Return true if handled (Enter/Esc/Tab are handled by the App otherwise).
  virtual bool OnKey(SDL_Keycode k) { (void)k; return false; }
  virtual void Update(double dt) { (void)dt; }
  virtual void OnEditChange(Widget& w) { (void)w; }
  // Extra drawing on top of the widgets (canvas = whole 800x600 frame).
  virtual void PaintOver(SDL_Surface* canvas, int ox, int oy) { (void)canvas; (void)ox; (void)oy; }

  // Popups only: closes this popup with a result (Delphi ModalResult).
  void Close(int result);
  bool IsPopup() const { return popup_; }

 private:
  friend class App;
  bool popup_ = false;
  std::function<void(int)> done_;
};

struct AppConfig {
  std::string title = "Menu";
  int width = 800, height = 600;
  bool fullscreen = false;
  std::string screenshot_path;  // F12 / scripted shots
};

class App {
  friend class Screen;
 public:
  explicit App(Skin& skin) : skin(skin) {}
  ~App();
  bool Init(const AppConfig& cfg);
  // Runs until Quit(). Returns the exit code.
  int Run();
  void Quit(int code = 0) { quit_ = true; exit_code_ = code; }

  // Replaces the current screen (and closes any popups).
  void Show(Screen* s);
  // Opens a modal popup; `done` gets its result when it closes.
  void ShowModal(Screen* s, std::function<void(int)> done = nullptr);
  Screen* Current() const { return cur_; }
  // Runs `fn` after `ms` milliseconds (Delphi TTimer-like, one shot).
  void After(int ms, std::function<void()> fn, const void* owner = nullptr);
  void CancelTimers(const void* owner);
  // Runs a child process while the window is hidden and audio paused;
  // returns its exit status (-1 if it could not be started).
  int RunChild(const std::vector<std::string>& argv);
  void Screenshot(const std::string& path);
  void SetFocus(Widget* w) { focus_ = w; }

  Skin& skin;
  Audio audio;
  std::function<void(Widget&)> on_button_down;  // click sound
  // Scripted input for tests: lines "<ms> click <name>", "<ms> key <name>",
  // "<ms> shot <path>", "<ms> quit".
  void LoadScript(const std::string& path);

 private:
  struct Timer {
    Uint64 at;
    std::function<void()> fn;
    const void* owner;
  };
  struct Step {
    Uint64 at;
    std::string cmd, arg;
  };
  void Draw();
  void DrawForm(Screen& s, int ox, int oy);
  Screen* Top() const { return popups_.empty() ? cur_ : popups_.back(); }
  void Origin(Screen& s, int* ox, int* oy) const;
  Widget* HitTest(int x, int y);
  void Click(Widget* w);
  void ActivateScreen(Screen* s);
  void FocusFirst(Screen& s);
  void MoveFocus(int dir);
  void RunScript();

  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  SDL_Surface* canvas_ = nullptr;
  Screen* cur_ = nullptr;
  std::vector<Screen*> popups_;
  std::vector<Timer> timers_;
  std::vector<Step> script_;
  Uint64 script_t0_ = 0;
  Widget* hover_ = nullptr;
  Widget* pressed_ = nullptr;
  Widget* focus_ = nullptr;
  bool quit_ = false;
  int exit_code_ = 0;
  AppConfig cfg_;
};

}  // namespace davi
