// TfrmDisplaySettings (Opties > Gevorderd). Het origineel stelde hier de
// videokaart, resolutie, helderheid en DirectX-probleemopties in; in de
// Mac-port zijn dat de beeld-upgrades uit graphics.ini (src/gfx_settings.h):
// resolutie, MSAA met alpha-to-coverage, mipmaps, anisotropic filtering en
// of de race upgraded start. De knoppen en labels komen uit
// mods/menu/layout/TfrmDisplaySettings.ini; elke knop wisselt door zijn
// waarden. Zoals in het origineel (0x47D980) gaat OK terug naar Opties, en
// Opties OK schrijft het weg.
#include "vracer/screens.h"

namespace vr {

namespace {

const char* kOwn[] = {"lblTitle", "lblResolution", "btnResolution", "lblMsaa", "btnMsaa", "lblA2c", "btnA2c",
                      "lblMipmaps", "btnMipmaps", "lblAniso", "btnAniso", "lblUpgrades", "btnUpgrades",
                      "lblKeys", "btnOk", "btnCancel"};

// Volgende waarde in `list` na `v` (rond).
template <typename T, size_t N>
T Next(const T (&list)[N], T v) {
  for (size_t i = 0; i < N; i++)
    if (list[i] == v) return list[(i + 1) % N];
  return list[0];
}

std::string OnOff(bool b) { return b ? "Aan" : "Uit"; }

}  // namespace

DisplaySettingsPopup::DisplaySettingsPopup(Game& g) : VrScreen(g, "TfrmDisplaySettings") {
  for (auto& w : form.widgets) {
    bool own = false;
    for (const char* n : kOwn) own = own || davi::Lower(w->name) == davi::Lower(n);
    if (!own) w->visible = false;
  }
  // POP_STD1 (456x206, knoppenbalk onderaan) hoger gemaakt: het vlak boven
  // de balk uitgerekt, de balk zelf ongewijzigd.
  form.w = 456;
  form.h = 330;
  if (SDL_Surface* std1 = g.assets.Bitmap("pop_std1")) {
    bg_ = SDL_CreateSurface(form.w, form.h, std1->format);
    const int band = 160;  // eerste rij van de knoppenbalk
    SDL_Rect top_src{0, 0, std1->w, band}, top_dst{0, 0, form.w, form.h - (std1->h - band)};
    SDL_BlitSurfaceScaled(std1, &top_src, bg_, &top_dst, SDL_SCALEMODE_LINEAR);
    SDL_Rect bar_src{0, band, std1->w, std1->h - band}, bar_dst{0, top_dst.h, form.w, std1->h - band};
    SDL_BlitSurface(std1, &bar_src, bg_, &bar_dst);
    form.background = bg_;
  }
}

DisplaySettingsPopup::~DisplaySettingsPopup() {
  if (bg_) SDL_DestroySurface(bg_);
}

void DisplaySettingsPopup::Edit(const vracer::GfxSettings& s) { settings = s; }

void DisplaySettingsPopup::OnShow() {
  Captions();
  game.app.SetFocus(form.Get("btnOk"));
}

void DisplaySettingsPopup::Captions() {
  const vracer::GfxSettings& s = settings;
  form.SetCaption("btnResolution", s.Screen() ? "Scherm" : s.Original() ? "Origineel" : s.ResolutionText());
  form.SetCaption("btnMsaa", s.msaa > 1 ? std::to_string(s.msaa) + "x" : "Uit");
  form.SetCaption("btnA2c", OnOff(s.alpha_to_coverage));
  form.SetCaption("btnMipmaps", OnOff(s.mipmaps));
  form.SetCaption("btnAniso", s.anisotropy > 1 ? std::to_string(s.anisotropy) + "x" : "Uit");
  form.SetCaption("btnUpgrades", s.upgrades ? "Upgraded" : "Origineel");
}

void DisplaySettingsPopup::OnClick(davi::Widget& w) {
  // Namen uit de layout-ini komen in kleine letters binnen.
  std::string n = davi::Lower(w.name);
  if (n.rfind("lbl", 0) == 0) n = "btn" + n.substr(3);  // een label klikt zijn knop
  vracer::GfxSettings& s = settings;
  if (n == "btnresolution") {
    static const char* kRes[] = {"screen", "1920x1440", "1280x960", "original"};
    std::string cur = s.ResolutionText();
    size_t i = 0;
    while (i < 4 && cur != kRes[i]) i++;
    s.SetResolution(kRes[(i + 1) % 4]);
  } else if (n == "btnmsaa") {
    static const int kMsaa[] = {1, 2, 4, 8};
    s.msaa = Next(kMsaa, s.msaa);
  } else if (n == "btna2c") {
    s.alpha_to_coverage = !s.alpha_to_coverage;
  } else if (n == "btnmipmaps") {
    s.mipmaps = !s.mipmaps;
  } else if (n == "btnaniso") {
    static const int kAniso[] = {1, 2, 4, 8, 16};
    s.anisotropy = Next(kAniso, s.anisotropy);
  } else if (n == "btnupgrades") {
    s.upgrades = !s.upgrades;
  } else if (n == "btnok") {
    Close(1);
    return;
  } else if (n == "btncancel") {
    Close(2);
    return;
  }
  Captions();
}

}  // namespace vr
