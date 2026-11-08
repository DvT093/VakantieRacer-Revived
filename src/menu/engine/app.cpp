#include "engine/app.h"

#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <sstream>

extern char** environ;

namespace davi {

Screen::Screen(App& a, const std::string& form_cls) : app(a) {
  std::string layout = a.skin.assets ? a.skin.assets->Find("layout/" + form_cls + ".ini") : "";
  built = form.Build(a.skin, form_cls, layout);
  if (a.skin.background_name) {
    std::string bg = a.skin.background_name(form_cls);
    if (!bg.empty() && a.skin.assets) {
      form.background = a.skin.assets->Bitmap(bg);
      if (Widget* w = form.Get("imgBackGround")) w->picture = form.background;
    }
  }
}

void Screen::Close(int result) {
  App& a = app;
  auto it = std::find(a.popups_.begin(), a.popups_.end(), this);
  if (it == a.popups_.end()) return;
  a.popups_.erase(it);
  OnHide();
  a.hover_ = a.pressed_ = nullptr;
  a.focus_ = nullptr;
  if (Screen* t = a.Top()) a.FocusFirst(*t);
  auto done = std::move(done_);
  done_ = nullptr;
  if (done) done(result);
}

App::~App() {
  audio.Shutdown();
  if (canvas_) SDL_DestroySurface(canvas_);
  if (tex_) SDL_DestroyTexture(tex_);
  if (ren_) SDL_DestroyRenderer(ren_);
  if (win_) SDL_DestroyWindow(win_);
  SDL_Quit();
}

bool App::Init(const AppConfig& cfg) {
  cfg_ = cfg;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return false;
  }
  audio.Init();
  // Window: the 800x600 menu, scaled up to fit ~80% of the display.
  int ww = cfg.width, wh = cfg.height;
  if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
    float sc = std::min(dm->w * 0.8f / cfg.width, dm->h * 0.8f / cfg.height);
    if (sc > 1.0f) ww = int(cfg.width * sc), wh = int(cfg.height * sc);
  }
  win_ = SDL_CreateWindow(cfg.title.c_str(), ww, wh,
                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                              (cfg.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
  if (!win_) return false;
  ren_ = SDL_CreateRenderer(win_, nullptr);
  if (!ren_) return false;
  SDL_SetRenderVSync(ren_, 1);
  SDL_SetRenderLogicalPresentation(ren_, cfg.width, cfg.height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, cfg.width, cfg.height);
  SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_LINEAR);
  canvas_ = SDL_CreateSurface(cfg.width, cfg.height, SDL_PIXELFORMAT_XRGB8888);
  return tex_ && canvas_;
}

void App::ActivateScreen(Screen* s) {
  hover_ = pressed_ = focus_ = nullptr;
  if (s) FocusFirst(*s);
}

void App::Show(Screen* s) {
  while (!popups_.empty()) {
    Screen* p = popups_.back();
    popups_.pop_back();
    p->OnHide();
    p->done_ = nullptr;
  }
  if (cur_ && cur_ != s) cur_->OnHide();
  cur_ = s;
  ActivateScreen(s);
  if (s) s->OnShow();
  if (s) FocusFirst(*s);  // OnShow may have changed what is enabled
}

void App::ShowModal(Screen* s, std::function<void(int)> done) {
  s->popup_ = true;
  s->done_ = std::move(done);
  popups_.push_back(s);
  ActivateScreen(s);
  s->OnShow();
  FocusFirst(*s);
}

void App::After(int ms, std::function<void()> fn, const void* owner) {
  timers_.push_back({SDL_GetTicks() + Uint64(std::max(0, ms)), std::move(fn), owner});
}

void App::CancelTimers(const void* owner) {
  timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [&](const Timer& t) { return t.owner == owner; }),
                timers_.end());
}

void App::FocusFirst(Screen& s) {
  Widget* best = nullptr;
  for (auto& w : s.form.widgets) {
    if (!w->visible || !w->enabled || !w->tab_stop) continue;
    if (w->kind != Widget::kButton && w->kind != Widget::kEdit && w->kind != Widget::kCheckBox &&
        w->kind != Widget::kRadio)
      continue;
    if (!best || w->tab_order < best->tab_order) best = w.get();
  }
  focus_ = best;
}

void App::MoveFocus(int dir) {
  Screen* s = Top();
  if (!s) return;
  std::vector<Widget*> v;
  for (auto& w : s->form.widgets)
    if (w->visible && w->enabled && w->tab_stop &&
        (w->kind == Widget::kButton || w->kind == Widget::kEdit || w->kind == Widget::kCheckBox ||
         w->kind == Widget::kRadio))
      v.push_back(w.get());
  if (v.empty()) return;
  std::stable_sort(v.begin(), v.end(), [](Widget* a, Widget* b) { return a->tab_order < b->tab_order; });
  auto it = std::find(v.begin(), v.end(), focus_);
  int i = it == v.end() ? 0 : int(it - v.begin()) + dir;
  focus_ = v[(i + int(v.size())) % int(v.size())];
}

void App::Origin(Screen& s, int* ox, int* oy) const {
  *ox = s.IsPopup() ? (cfg_.width - s.form.w) / 2 : 0;
  *oy = s.IsPopup() ? (cfg_.height - s.form.h) / 2 : 0;
}

Widget* App::HitTest(int x, int y) {
  Screen* s = Top();
  if (!s) return nullptr;
  int ox, oy;
  Origin(*s, &ox, &oy);
  x -= ox;
  y -= oy;
  for (auto it = s->form.widgets.rbegin(); it != s->form.widgets.rend(); ++it) {
    Widget* w = it->get();
    if (!w->visible) continue;
    bool clickable = w->kind == Widget::kButton || w->kind == Widget::kCheckBox ||
                     w->kind == Widget::kRadio || w->kind == Widget::kEdit || !w->onclick.empty();
    if (!clickable) continue;
    if (x >= w->r.x && x < w->r.x + w->r.w && y >= w->r.y && y < w->r.y + w->r.h) return w->enabled ? w : nullptr;
  }
  return nullptr;
}

void App::Click(Widget* w) {
  Screen* s = Top();
  if (!w || !s || !w->enabled || !w->visible) return;
  if (w->kind == Widget::kCheckBox) w->checked = !w->checked;
  if (w->kind == Widget::kRadio) {
    for (auto& o : s->form.widgets)
      if (o->kind == Widget::kRadio && o->group == w->group) o->checked = false;
    w->checked = true;
  }
  s->OnClick(*w);
}

void App::DrawForm(Screen& s, int ox, int oy) {
  SDL_Rect clip{ox, oy, s.form.w, s.form.h};
  SDL_SetSurfaceClipRect(canvas_, &clip);
  for (auto& wp : s.form.widgets) {
    Widget& w = *wp;
    if (!w.visible) continue;
    SDL_Rect r{w.r.x + ox, w.r.y + oy, w.r.w, w.r.h};
    switch (w.kind) {
      case Widget::kImage: {
        SDL_Surface* p = w.Pic();
        if (!p) break;
        if (w.pic_transparent && !w.owned) {
          // Delphi TBitmap transparency: the bottom-left pixel's colour,
          // unless the program set TransparentColor.
          Uint32 key = w.key_rgb >= 0
                           ? SDL_MapSurfaceRGB(p, Uint8(w.key_rgb >> 16), Uint8(w.key_rgb >> 8), Uint8(w.key_rgb))
                           : ((Uint32*)((Uint8*)p->pixels + (p->h - 1) * p->pitch))[0];
          SDL_SetSurfaceColorKey(p, true, key);
        }
        if (w.stretch) {
          SDL_BlitSurfaceScaled(p, nullptr, canvas_, &r, SDL_SCALEMODE_NEAREST);
        } else {
          SDL_Rect dst = r;
          if (w.center) dst.x += (w.r.w - p->w) / 2, dst.y += (w.r.h - p->h) / 2;
          SDL_Rect src{0, 0, std::min(p->w, w.r.w ? w.r.w : p->w), std::min(p->h, w.r.h ? w.r.h : p->h)};
          SDL_BlitSurface(p, &src, canvas_, &dst);
        }
        break;
      }
      case Widget::kLabel: {
        TextStyle st{skin.GetFont(w.font.font), w.font.px, w.enabled ? w.font.color : w.disabled_color,
                     w.halign, w.valign, w.wrap};
        SDL_Rect tr = r;
        if (w.autosize && !w.wrap && st.font) {
          tr.w = std::max(tr.w, st.font->Width(w.caption, w.font.px) + 2);
          tr.h = std::max(tr.h, st.font->LineHeight(w.font.px) * 1);
        }
        DrawText(canvas_, tr, w.caption, st);
        break;
      }
      case Widget::kEdit: {
        SDL_Rect in = r;
        if (w.border) {
          SDL_FillSurfaceRect(canvas_, &r, SDL_MapSurfaceRGB(canvas_, 0x80, 0x80, 0x80));
          in = {r.x + 1, r.y + 1, r.w - 2, r.h - 2};
        }
        SDL_FillSurfaceRect(canvas_, &in, SDL_MapSurfaceRGB(canvas_, w.color.r, w.color.g, w.color.b));
        if (w.select_all && focus_ == &w && !w.caption.empty() && skin.GetFont(w.font.font)) {
          SDL_Rect sel{r.x + 3, r.y + 2, skin.GetFont(w.font.font)->Width(w.caption, w.font.px) + 2, r.h - 4};
          SDL_FillSurfaceRect(canvas_, &sel, SDL_MapSurfaceRGB(canvas_, 0, 0, 0x80));
        }
        SDL_Color tc = w.select_all && focus_ == &w ? SDL_Color{255, 255, 255, 255} : w.font.color;
        TextStyle st{skin.GetFont(w.font.font), w.font.px, tc, HAlign::kLeft, VAlign::kCenter, false};
        SDL_Rect tr{r.x + 4, r.y, r.w - 8, r.h};
        std::string t = w.caption;
        if (focus_ == &w && !w.select_all && (SDL_GetTicks() / 500) % 2 == 0) t += "|";
        DrawText(canvas_, tr, t, st);
        break;
      }
      case Widget::kButton:
      case Widget::kCheckBox:
      case Widget::kRadio: {
        if (!w.def) break;
        GraphicDef::State state = GraphicDef::kUp;
        if (!w.enabled) state = GraphicDef::kDisabled;
        else if (pressed_ == &w && hover_ == &w) state = GraphicDef::kDown;
        else if (hover_ == &w) state = GraphicDef::kHovered;
        else if (focus_ == &w) state = GraphicDef::kFocused;
        if (w.kind != Widget::kButton) state = w.checked ? GraphicDef::kDown : GraphicDef::kUp;
        if (SDL_Surface* img = w.def->Get(state)) {
          SDL_Rect src{0, 0, std::min(img->w, w.r.w), std::min(img->h, w.r.h)};
          SDL_BlitSurface(img, &src, canvas_, &r);
        }
        if (!w.caption.empty()) {
          SDL_Rect cr{0, 0, w.r.w, w.r.h};
          auto it = skin.caption_rects.find(w.def->caption_rect);
          if (it != skin.caption_rects.end()) cr = it->second(w);
          cr.x += r.x;
          cr.y += r.y;
          // DrawText: DT_CENTER|DT_VCENTER|DT_SINGLELINE, or DT_CENTER|DT_WORDBREAK (top).
          TextStyle st{skin.GetFont(w.font.font), w.font.px, w.enabled ? w.font.color : w.disabled_color,
                       HAlign::kCenter, w.wrap ? VAlign::kTop : VAlign::kCenter, w.wrap};
          DrawText(canvas_, cr, w.caption, st);
        }
        break;
      }
      default:
        break;
    }
  }
  s.PaintOver(canvas_, ox, oy);
  SDL_SetSurfaceClipRect(canvas_, nullptr);
}

void App::Draw() {
  SDL_FillSurfaceRect(canvas_, nullptr, 0);
  if (cur_) DrawForm(*cur_, 0, 0);
  for (Screen* p : popups_) {
    int ox, oy;
    Origin(*p, &ox, &oy);
    if (p->form.background) {
      SDL_Rect dst{ox, oy, p->form.w, p->form.h};
      SDL_BlitSurface(p->form.background, nullptr, canvas_, &dst);
    }
    DrawForm(*p, ox, oy);
  }
  SDL_UpdateTexture(tex_, nullptr, canvas_->pixels, canvas_->pitch);
  SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
  SDL_RenderClear(ren_);
  SDL_RenderTexture(ren_, tex_, nullptr, nullptr);
  SDL_RenderPresent(ren_);
}

void App::Screenshot(const std::string& path) {
  Draw();
  if (!SDL_SavePNG(canvas_, path.c_str())) SDL_Log("screenshot %s: %s", path.c_str(), SDL_GetError());
}

void App::LoadScript(const std::string& path) {
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream ss(line);
    Step s{};
    if (!(ss >> s.at >> s.cmd)) continue;
    std::getline(ss, s.arg);
    s.arg = Trim(s.arg);
    script_.push_back(s);
  }
  std::stable_sort(script_.begin(), script_.end(), [](const Step& a, const Step& b) { return a.at < b.at; });
  script_t0_ = SDL_GetTicks();
}

void App::RunScript() {
  while (!script_.empty() && SDL_GetTicks() - script_t0_ >= script_.front().at) {
    Step s = script_.front();
    script_.erase(script_.begin());
    Screen* t = Top();
    if (s.cmd == "click" && t) {
      Widget* w = t->form.Get(s.arg);
      if (!w) SDL_Log("script: no %s on %s", s.arg.c_str(), t->form.cls.c_str());
      else {
        if (on_button_down && w->kind == Widget::kButton) on_button_down(*w);
        Click(w);
      }
    } else if (s.cmd == "key" && t) {
      SDL_Keycode k = SDL_GetKeyFromName(s.arg.c_str());
      if (!t->OnKey(k)) {
        if (k == SDLK_ESCAPE)
          for (auto& w : t->form.widgets)
            if (w->is_cancel && w->visible && w->enabled) { Click(w.get()); break; }
      }
    } else if (s.cmd == "type" && t) {
      if (focus_ && focus_->kind == Widget::kEdit) {
        if (focus_->select_all) focus_->caption.clear(), focus_->select_all = false;
        focus_->caption += s.arg;
        t->OnEditChange(*focus_);
      }
    } else if (s.cmd == "shot") {
      Screenshot(s.arg);
    } else if (s.cmd == "hover" && t) {
      hover_ = t->form.Get(s.arg);
    } else if (s.cmd == "quit") {
      Quit(0);
    }
  }
}

int App::Run() {
  Uint64 last = SDL_GetTicks();
  while (!quit_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      Screen* t = Top();
      switch (e.type) {
        case SDL_EVENT_QUIT:
          quit_ = true;
          break;
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          SDL_ConvertEventToRenderCoordinates(ren_, &e);
          float fx = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
          float fy = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
          Widget* w = HitTest(int(fx), int(fy));
          hover_ = w;
          if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            pressed_ = w;
            if (w && (w->kind == Widget::kButton || w->kind == Widget::kEdit || w->kind == Widget::kCheckBox ||
                      w->kind == Widget::kRadio) && w->tab_stop)
              focus_ = w;
            if (w && on_button_down && w->kind == Widget::kButton) on_button_down(*w);
          } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
            Widget* p = pressed_;
            pressed_ = nullptr;
            if (p && p == w) Click(p);
          }
          break;
        }
        case SDL_EVENT_TEXT_INPUT:
          if (focus_ && focus_->kind == Widget::kEdit) {
            if (focus_->select_all) focus_->caption.clear(), focus_->select_all = false;
            focus_->caption += Utf8ToCp1252(e.text.text);
            if (focus_->max_len > 0 && int(focus_->caption.size()) > focus_->max_len)
              focus_->caption.resize(focus_->max_len);
            if (Screen* t = Top()) t->OnEditChange(*focus_);
          }
          break;
        case SDL_EVENT_KEY_DOWN: {
          if (!t) break;
          SDL_Keycode k = e.key.key;
          if (k == SDLK_F12 && !cfg_.screenshot_path.empty()) {
            Screenshot(cfg_.screenshot_path);
            break;
          }
          if (k == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)) {
            bool fs = SDL_GetWindowFlags(win_) & SDL_WINDOW_FULLSCREEN;
            SDL_SetWindowFullscreen(win_, !fs);
            break;
          }
          if (focus_ && focus_->kind == Widget::kEdit && k == SDLK_BACKSPACE) {
            if (focus_->select_all) focus_->caption.clear(), focus_->select_all = false;
            else if (!focus_->caption.empty()) focus_->caption.pop_back();
            t->OnEditChange(*focus_);
            break;
          }
          if (t->OnKey(k)) break;
          if (k == SDLK_TAB) {
            MoveFocus(e.key.mod & SDL_KMOD_SHIFT ? -1 : 1);
          } else if (k == SDLK_ESCAPE) {
            for (auto& w : t->form.widgets)
              if (w->is_cancel && w->visible && w->enabled) {
                Click(w.get());
                break;
              }
          } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
            Widget* target = focus_ && focus_->kind != Widget::kEdit ? focus_ : nullptr;
            if (!target && k != SDLK_SPACE)
              for (auto& w : t->form.widgets)
                if (w->is_default && w->visible && w->enabled) target = w.get();
            if (target) {
              if (on_button_down && target->kind == Widget::kButton) on_button_down(*target);
              Click(target);
            }
          }
          break;
        }
        default:
          break;
      }
    }
    // Text input only while an edit has focus.
    bool want_text = focus_ && focus_->kind == Widget::kEdit;
    if (want_text != SDL_TextInputActive(win_)) {
      if (want_text) SDL_StartTextInput(win_);
      else SDL_StopTextInput(win_);
    }
    Uint64 now = SDL_GetTicks();
    for (size_t i = 0; i < timers_.size();) {
      if (now >= timers_[i].at) {
        auto fn = std::move(timers_[i].fn);
        timers_.erase(timers_.begin() + i);
        fn();
        i = 0;
      } else {
        i++;
      }
    }
    RunScript();
    double dt = double(now - last) / 1000.0;
    last = now;
    if (cur_) cur_->Update(dt);
    for (Screen* p : std::vector<Screen*>(popups_)) p->Update(dt);
    audio.Update();
    Draw();
  }
  return exit_code_;
}

int App::RunChild(const std::vector<std::string>& argv) {
  std::vector<char*> args;
  for (auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  audio.Pause(true);
  pid_t pid;
  int rc = posix_spawn(&pid, args[0], nullptr, nullptr, args.data(), environ);
  if (rc != 0) {
    SDL_Log("menu: cannot start %s (%d)", args[0], rc);
    audio.Pause(false);
    return -1;
  }
  SDL_HideWindow(win_);
  int status = 0;
  for (;;) {
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid || r < 0) break;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) kill(pid, SIGTERM);
    }
    SDL_Delay(50);
  }
  SDL_ShowWindow(win_);
  SDL_RaiseWindow(win_);
  audio.Pause(false);
  hover_ = pressed_ = nullptr;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

}  // namespace davi
