// A menu screen built from a Delphi form: the original layout (DFM in the
// user's EXE), the captions/fonts from the game's MenuCaps-style ini, and an
// optional mod layout file that moves, hides or adds components.
#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/assets.h"
#include "engine/dfm.h"
#include "engine/ini.h"
#include "engine/text.h"

namespace davi {

// TDaviGraphicDef: one bitmap per button state.
struct GraphicDef {
  enum State { kUp, kDown, kFocused, kHovered, kDisabled, kCount };
  SDL_Surface* img[kCount] = {};
  int w = 0, h = 0;
  std::string caption_rect;  // OnGetCaptionRect handler name ("" = whole button)
  SDL_Surface* Get(State s) const;
};

struct FontSpec {
  int font = 1;  // index into Skin::fonts (MenuCaps "Font=")
  int px = 16;
  SDL_Color color{0, 0, 0, 255};
};

struct Widget {
  // kThumb (port, layout Class=TDaviThumb): a clickable picture in a rounded
  // frame, `checked` = the chosen one of its `group` (arrow keys walk the
  // group); `caption` when there is no picture.
  enum Kind { kOther, kImage, kLabel, kButton, kCheckBox, kRadio, kEdit, kSlider, kThumb };
  Kind kind = kOther;
  std::string cls, name;
  SDL_Rect r{0, 0, 0, 0};
  bool visible = true, enabled = true;
  int tab_order = 1000;
  bool tab_stop = true, is_cancel = false, is_default = false;
  // Text (labels, buttons, edits).
  std::string caption;
  FontSpec font;
  SDL_Color disabled_color{75, 97, 99, 255};
  bool wrap = false, autosize = true, transparent = true;
  HAlign halign = HAlign::kLeft;
  VAlign valign = VAlign::kTop;
  // Edits.
  SDL_Color color{255, 255, 255, 255};
  bool border = true, select_all = false;
  int max_len = 0;
  // Images.
  SDL_Surface* picture = nullptr;  // not owned
  SDL_Surface* owned = nullptr;    // owned canvas (drawn on by flow code)
  bool stretch = false, center = false, pic_transparent = false;
  int key_rgb = -1;  // transparent colour 0xRRGGBB (default: bottom-left pixel)
  // Buttons.
  const GraphicDef* def = nullptr;
  bool checked = false;  // check boxes / radio buttons
  int group = 0;
  std::string onclick;   // DFM OnClick handler name
  // Sliders (TDaviGraphicSlider): picture = track, def = needle.
  int min = 0, max = 100, pos = 0, margin = 0;
  Widget() = default;
  Widget(const Widget&) = delete;
  Widget& operator=(const Widget&) = delete;
  ~Widget();
  SDL_Surface* Pic() const { return owned ? owned : picture; }
  // Makes an owned canvas of the widget's size (copy of `bg` under it).
  SDL_Surface* Canvas(SDL_Surface* bg);
};

// Shared look: fonts, button definitions, caption strings.
struct Skin {
  DelphiExe* exe = nullptr;
  Assets* assets = nullptr;
  LayeredIni caps;                          // MenuCaps.ini (+ mod layer)
  std::map<int, std::unique_ptr<Font>> fonts;  // MenuCaps Font index -> font
  std::map<std::string, std::unique_ptr<GraphicDef>> defs;  // "TData.defBtn"
  std::map<std::string, std::vector<SDL_Surface*>> owned_bmps;
  // OnGetCaptionRect handlers by name: button rect (0-based) -> caption rect.
  std::map<std::string, std::function<SDL_Rect(const Widget&)>> caption_rects;
  std::string data_module = "TData";  // where "Data.xxx" references live
  // Background bitmap name for a form class ("" = none), see Screen.
  std::function<std::string(const std::string& cls)> background_name;

  const GraphicDef* Def(const std::string& form, const std::string& ref);
  Font* GetFont(int idx);
  ~Skin();
};

class Form {
 public:
  std::string cls;
  int w = 800, h = 600;
  std::vector<std::unique_ptr<Widget>> widgets;  // z-order: first = bottom
  SDL_Surface* background = nullptr;             // imgBackGround picture

  // Builds the form `cls` from the EXE. `mod_layout` is an optional ini path
  // (see CLAUDE.md "Menu mods") applied last.
  bool Build(Skin& skin, const std::string& cls, const std::string& mod_layout);
  Widget* Get(const std::string& name);  // nullptr if absent
  Widget& W(const std::string& name);     // a dummy if absent (logged)
  void SetCaption(const std::string& name, const std::string& text);
  void SetVisible(const std::string& name, bool v);
  void SetEnabled(const std::string& name, bool v);
  void SetPicture(const std::string& name, SDL_Surface* s);

 private:
  std::unique_ptr<Widget> dummy_;
  void Add(Skin& skin, const Component& c, int ox, int oy);
};

}  // namespace davi
