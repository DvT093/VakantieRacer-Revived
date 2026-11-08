#include "engine/form.h"

#include <cmath>
#include <cstdlib>

namespace davi {

SDL_Surface* GraphicDef::Get(State s) const {
  if (img[s]) return img[s];
  return img[kUp];
}

Widget::~Widget() {
  if (owned) SDL_DestroySurface(owned);
}

SDL_Surface* Widget::Canvas(SDL_Surface* bg) {
  if (!owned || owned->w != r.w || owned->h != r.h) {
    if (owned) SDL_DestroySurface(owned);
    owned = SDL_CreateSurface(std::max(1, r.w), std::max(1, r.h), SDL_PIXELFORMAT_XRGB8888);
  }
  if (bg) {
    SDL_Rect src = r;
    SDL_BlitSurface(bg, &src, owned, nullptr);
  }
  return owned;
}

Skin::~Skin() {
  for (auto& [_, d] : defs)
    for (auto* s : d->img)
      if (s) SDL_DestroySurface(s);
  for (auto& [_, v] : owned_bmps)
    for (auto* s : v) SDL_DestroySurface(s);
}

Font* Skin::GetFont(int idx) {
  auto it = fonts.find(idx);
  if (it != fonts.end()) return it->second.get();
  return fonts.empty() ? nullptr : fonts.begin()->second.get();
}

const GraphicDef* Skin::Def(const std::string& form, const std::string& ref) {
  if (ref.empty()) return nullptr;
  std::string owner = form, name = ref;
  size_t dot = ref.find('.');
  if (dot != std::string::npos) {
    owner = ref.substr(0, dot) == "Data" ? data_module : "T" + ref.substr(0, dot);
    name = ref.substr(dot + 1);
  }
  std::string key = owner + "." + name;
  auto it = defs.find(key);
  if (it != defs.end()) return it->second.get();
  auto f = exe ? exe->Form(owner) : nullptr;
  Component* c = f ? f->Find(name) : nullptr;
  auto d = std::make_unique<GraphicDef>();
  if (c) {
    d->w = c->Int("Width");
    d->h = c->Int("Height");
    d->caption_rect = c->Str("OnGetCaptionRect");
    static const char* links[] = {"UpLink", "DownLink", "FocusedLink", "HoveredLink", "DisabledLink"};
    for (int s = 0; s < GraphicDef::kCount; s++) {
      // A mod may replace a state picture: "<def>.<state>.bmp", e.g. defBtn.UpLink.bmp.
      SDL_Surface* m = nullptr;
      if (assets) {
        std::string p = assets->Find(name + "." + links[s] + ".bmp");
        if (!p.empty() && p.find("/MenuData/") == std::string::npos) m = LoadBmp(ReadFile(p));
      }
      if (!m) {
        const Value* v = c->Prop(std::string(links[s]) + ".DIB");
        if (v && v->kind == Value::kBytes && v->bytes.size() > 54) m = LoadBmp(v->bytes);
        else if (const Value* fn = c->Prop(std::string(links[s]) + ".Filename"); fn && !fn->s.empty() && assets)
          m = LoadBmp(ReadFile(assets->Find(fn->s)));
      }
      d->img[s] = m;
    }
  } else {
    SDL_Log("menu: graphic definition %s not found", key.c_str());
  }
  return (defs[key] = std::move(d)).get();
}

namespace {

SDL_Color ColorRef(long v) {
  return SDL_Color{Uint8(v & 0xff), Uint8((v >> 8) & 0xff), Uint8((v >> 16) & 0xff), 255};
}

// Delphi colour identifiers used in these forms.
SDL_Color DelphiColor(const Value& v, SDL_Color def) {
  if (v.kind == Value::kInt) return ColorRef(long(v.i));
  static const std::map<std::string, long> named = {
      {"clBlack", 0x000000}, {"clWhite", 0xFFFFFF}, {"clSilver", 0xC0C0C0}, {"clGray", 0x808080},
      {"clRed", 0x0000FF},   {"clLime", 0x00FF00},  {"clBlue", 0xFF0000},   {"clYellow", 0x00FFFF},
      {"clNavy", 0x800000},  {"clMaroon", 0x000080}, {"clWindowText", 0x000000}, {"clWindow", 0xFFFFFF}, {"clBtnFace", 0xC0C0C0}, {"clFuchsia", 0xFF00FF}};
  auto it = named.find(v.s);
  return it == named.end() ? def : ColorRef(it->second);
}

int PointsToPx(int pt) { return int(std::lround(pt * 96.0 / 72.0)); }

long ParseNum(const std::string& s) { return std::strtol(s.c_str(), nullptr, 0); }

bool NonVisual(const std::string& cls) {
  static const char* nv[] = {"TDaviGraphicDef", "TTimer", "TDaviWave", "TDaviEmbeddedFont",
                             "TDaviContextHelp", "TDaviWavPlayer", "TMainMenu", "TPopupMenu",
                             "TMediaPlayer"};
  for (auto* n : nv)
    if (cls == n) return true;
  return false;
}

}  // namespace

void Form::Add(Skin& skin, const Component& c, int ox, int oy) {
  if (NonVisual(c.cls)) return;
  auto w = std::make_unique<Widget>();
  w->cls = c.cls;
  w->name = c.name;
  w->r = {ox + c.Int("Left"), oy + c.Int("Top"), c.Int("Width"), c.Int("Height")};
  if (c.Str("Align") == "alClient") w->r = {0, 0, this->w, this->h};
  w->visible = c.Bool("Visible", true);
  w->enabled = c.Bool("Enabled", true);
  w->tab_order = c.Int("TabOrder", 1000);
  w->tab_stop = c.Bool("TabStop", true);
  w->is_cancel = c.Bool("Cancel");
  w->is_default = c.Bool("Default");
  w->caption = c.Str("Caption");
  w->wrap = c.Bool("WordWrap");
  w->autosize = c.Bool("AutoSize", c.cls == "TLabel");
  w->onclick = c.Str("OnClick");
  std::string al = c.Str("Alignment");
  if (al == "taCenter") w->halign = HAlign::kCenter;
  if (al == "taRightJustify") w->halign = HAlign::kRight;
  std::string lay = c.Str("Layout");
  if (lay == "tlCenter") w->valign = VAlign::kCenter;
  if (lay == "tlBottom") w->valign = VAlign::kBottom;
  // DFM font (overridden by the caption ini below).
  w->font.px = std::abs(c.Int("Font.Height", -11));
  if (const Value* col = c.Prop("Font.Color")) w->font.color = DelphiColor(*col, w->font.color);
  bool is_button = c.cls.find("Button") != std::string::npos || c.cls.find("CheckBox") != std::string::npos;

  if (c.cls == "TImage" || c.cls == "TDaviImage" || c.cls == "TRacerAnimatedImage") {
    w->kind = Widget::kImage;
    w->stretch = c.Bool("Stretch");
    w->center = c.Bool("Center");
    w->pic_transparent = c.Bool("Transparent");
    w->autosize = c.Bool("AutoSize");
    if (const Value* tc = c.Prop("TransparentColor")) {  // TDaviImage: always keyed
      SDL_Color k = DelphiColor(*tc, SDL_Color{255, 0, 255, 255});
      w->pic_transparent = true;
      w->key_rgb = (k.r << 16) | (k.g << 8) | k.b;
    }
    std::vector<uint8_t> bmp;
    if (const Value* p = c.Prop("Picture.Data")) bmp = PictureBytes(*p);
    if (const Value* p = c.Prop("LinkedDIB.DIB"); p && bmp.empty()) bmp = p->bytes;
    if (SDL_Surface* s = LoadBmp(bmp)) {
      skin.owned_bmps[cls].push_back(s);
      w->picture = s;
      if (w->autosize || w->r.w == 0) w->r.w = s->w, w->r.h = s->h;
    }
  } else if (c.cls == "TDaviGraphicSlider") {
    w->kind = Widget::kSlider;
    w->min = c.Int("Min", 0);
    w->max = c.Int("Max", 100);
    w->pos = c.Int("Position", 0);
    w->margin = c.Int("MarginHorz", 0);
    w->onclick = c.Str("OnChange");
    if (const Value* p = c.Prop("BackImage.Data"))
      if (SDL_Surface* s = LoadBmp(SizedBytes(*p))) {
        skin.owned_bmps[cls].push_back(s);
        w->picture = s;
      }
    w->def = skin.Def(cls, c.Str("Definition"));
  } else if (c.cls == "TLabel") {
    w->kind = Widget::kLabel;
  } else if (c.cls == "TEdit") {
    w->kind = Widget::kEdit;
    w->max_len = c.Int("MaxLength");
    w->border = c.Str("BorderStyle") != "bsNone";
    if (const Value* col = c.Prop("Color")) w->color = DelphiColor(*col, w->color);
  } else if (c.cls == "TDaviGraphicRadioButton") {
    w->kind = Widget::kRadio;
  } else if (c.cls == "TDaviGraphicCheckBox") {
    w->kind = Widget::kCheckBox;
  } else if (is_button) {
    w->kind = Widget::kButton;
  }
  if (w->kind == Widget::kButton || w->kind == Widget::kCheckBox || w->kind == Widget::kRadio) {
    w->def = skin.Def(cls, c.Str("Definition"));
    if (w->def && (w->r.w == 0 || w->r.h == 0)) w->r.w = w->def->w, w->r.h = w->def->h;
    w->checked = c.Bool("Checked");
    w->group = c.Int("GroupIndex");
  }

  // Captions, fonts and positions from the caption ini ([DEFAULT] + [<form>]).
  if (w->kind == Widget::kLabel || w->kind == Widget::kButton || w->kind == Widget::kCheckBox ||
      w->kind == Widget::kRadio || w->kind == Widget::kEdit || w->kind == Widget::kImage) {
    auto& caps = skin.caps;
    auto get = [&](const std::string& prop, std::string* out) {
      std::string k = c.name + "." + prop;
      if (caps.Has(cls, k)) { *out = caps.Get(cls, k); return true; }
      return false;
    };
    std::string v;
    // TCaptionForm.Create (0x490b88): [<form>] <comp>.<prop>, else
    // [DEFAULT], else the hard defaults. Font 1 = MenuFont1, other = MenuFont2.
    if (w->kind == Widget::kEdit) {
      w->font.font = 2;  // MS Sans Serif in the DFM; only the size is set
      w->font.px = PointsToPx(get("FontSize", &v) ? int(ParseNum(v)) : caps.GetInt("DEFAULT", "FontSize", 10));
    } else if (w->kind != Widget::kImage) {
      bool btn = w->kind != Widget::kLabel;
      w->caption = get("Caption", &v) ? v : caps.Get("DEFAULT", "Caption", "<No Value>");
      w->font.font = (get("Font", &v) ? int(ParseNum(v)) : caps.GetInt("DEFAULT", "Font", 1)) == 1 ? 1 : 2;
      int pt = get("FontSize", &v) ? int(ParseNum(v))
                                   : caps.GetInt("DEFAULT", btn ? "BtnFontSize" : "FontSize", 10);
      w->font.px = PointsToPx(pt);
      w->font.color = ColorRef(ParseNum(get("FontColor", &v) ? v
                                        : caps.Get("DEFAULT", btn ? "BtnFontColor" : "FontColor", "0xFFFFFF")));
      w->disabled_color = ColorRef(ParseNum(
          get("DisabledFontColor", &v) ? v : caps.Get("DEFAULT", "DisabledFontColor", "0xC0C0C0")));
    }
    if (get("Left", &v)) w->r.x = ox + int(ParseNum(v));
    if (get("Top", &v)) w->r.y = oy + int(ParseNum(v));
  }
  int cx = w->r.x, cy = w->r.y;
  widgets.push_back(std::move(w));
  for (auto& k : c.children) Add(skin, *k, cx, cy);
}

bool Form::Build(Skin& skin, const std::string& form_cls, const std::string& mod_layout) {
  cls = form_cls;
  auto root = skin.exe ? skin.exe->Form(form_cls) : nullptr;
  if (!root) {
    SDL_Log("menu: form %s not found in the EXE", form_cls.c_str());
    return false;
  }
  w = root->Int("ClientWidth", root->Int("Width", 800));
  h = root->Int("ClientHeight", root->Int("Height", 600));
  for (std::string p = form_cls; !p.empty(); p = skin.exe->ParentClass(p))
    if (p == "TGameMenuScreen") w = 800, h = 600;  // FormCreate: SetBounds(..., 800, 600)
  for (auto& c : root->children) Add(skin, *c, 0, 0);

  // Mod layout: one section per component (existing or new).
  IniFile mod;
  if (!mod_layout.empty() && mod.Load(mod_layout)) {
    for (auto& sec : mod.Sections()) {
      Widget* wd = Get(sec);
      if (!wd) {
        Component c;
        c.cls = mod.Get(sec, "Class", "TLabel");
        c.name = sec;
        Add(skin, c, 0, 0);
        wd = widgets.back().get();
        wd->name = sec;
      }
      auto num = [&](const char* k, int* out) {
        if (mod.Has(sec, k)) *out = int(ParseNum(mod.Get(sec, k)));
      };
      num("Left", &wd->r.x);
      num("Top", &wd->r.y);
      num("Width", &wd->r.w);
      num("Height", &wd->r.h);
      num("TabOrder", &wd->tab_order);
      num("Font", &wd->font.font);
      if (mod.Has(sec, "FontSize")) wd->font.px = PointsToPx(mod.GetInt(sec, "FontSize"));
      if (mod.Has(sec, "FontColor")) wd->font.color = ColorRef(ParseNum(mod.Get(sec, "FontColor")));
      if (mod.Has(sec, "Visible")) wd->visible = mod.GetInt(sec, "Visible") != 0;
      if (mod.Has(sec, "Enabled")) wd->enabled = mod.GetInt(sec, "Enabled") != 0;
      if (mod.Has(sec, "WordWrap")) wd->wrap = mod.GetInt(sec, "WordWrap") != 0;
      if (mod.Has(sec, "Caption")) wd->caption = mod.Get(sec, "Caption");
      if (mod.Has(sec, "Alignment")) {
        std::string a = Lower(mod.Get(sec, "Alignment"));
        wd->halign = a == "center" ? HAlign::kCenter : a == "right" ? HAlign::kRight : HAlign::kLeft;
      }
      if (mod.Has(sec, "Definition")) {
        wd->def = skin.Def(form_cls, mod.Get(sec, "Definition"));
        if (wd->def && !mod.Has(sec, "Width")) wd->r.w = wd->def->w, wd->r.h = wd->def->h;
      }
      if (mod.Has(sec, "Picture") && skin.assets) {
        wd->picture = skin.assets->Bitmap(mod.Get(sec, "Picture"));
        if (wd->picture && !mod.Has(sec, "Width")) wd->r.w = wd->picture->w, wd->r.h = wd->picture->h;
      }
      if (mod.Has(sec, "OnClick")) wd->onclick = mod.Get(sec, "OnClick");
    }
  }
  return true;
}

Widget* Form::Get(const std::string& name) {
  for (auto& w : widgets)
    if (Lower(w->name) == Lower(name)) return w.get();
  return nullptr;
}

Widget& Form::W(const std::string& name) {
  if (Widget* w = Get(name)) return *w;
  SDL_Log("menu: %s has no component %s", cls.c_str(), name.c_str());
  dummy_ = std::make_unique<Widget>();
  return *dummy_;
}

void Form::SetCaption(const std::string& n, const std::string& t) { W(n).caption = t; }
void Form::SetVisible(const std::string& n, bool v) { W(n).visible = v; }
void Form::SetEnabled(const std::string& n, bool v) { W(n).enabled = v; }
void Form::SetPicture(const std::string& n, SDL_Surface* s) {
  Widget& w = W(n);
  w.picture = s;
  if (s && w.kind == Widget::kImage && (w.autosize || !w.stretch)) w.r.w = s->w, w.r.h = s->h;
}

}  // namespace davi
