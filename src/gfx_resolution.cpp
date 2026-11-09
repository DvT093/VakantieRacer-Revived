// Eigen rendermaat voor de race (graphics.ini Resolution): de game rendert
// zelf op elke 4:3-maat met meeschalende HUD, hij hoeft alleen een andere
// maat te krijgen dan zijn modustabel heeft. Zie re/findings.md
// ("Resolutie").

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "x86/runtime.h"

namespace vracer {

// Gezet door main.cpp: >0 = de maat, <0 = uit (de modustabel).
int g_render_w = -1, g_render_h = -1;

// De grootste 4:3-maat (pixels) op het scherm: het hele scherm als de
// race fullscreen draait, anders het bruikbare deel min de titelbalk.
// Vanuit het menu: de grootste 4:3-maat in het menuvenster.
// Vóór recomp::RunProcess: SDL's video staat dan nog uit, dus even aan en
// weer uit (zonder signal handlers, zoals SwARc hem start).
void ScreenSize(int* w, int* h) {
  *w = 1024;
  *h = 768;
  // Started by the menu: the race window takes the menu window's place
  // (SwARc's VRACER_WINDOW=x,y,w,h,fullscreen,density in points), so the
  // largest 4:3 size inside that window.
  if (const char* e = std::getenv("VRACER_WINDOW")) {
    int x, y, pw, ph, fs = 0;
    float d = 1;
    if (std::sscanf(e, "%d,%d,%d,%d,%d,%f", &x, &y, &pw, &ph, &fs, &d) >= 4 && pw > 0 && ph > 0) {
      if (!(d > 0)) d = 1;
      int ww = int(pw * d), hh = int(ph * d);
      hh = std::min(hh, ww * 3 / 4) & ~1;
      *h = hh;
      *w = (hh * 4 / 3) & ~1;
      return;
    }
  }
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return;
  SDL_DisplayID d = SDL_GetPrimaryDisplay();
  const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(d);
  float density = m && m->pixel_density > 0 ? m->pixel_density : 1.0f;
  const char* fs = std::getenv("VRACER_FULLSCREEN");
  const char* bg = std::getenv("VRACER_BACKGROUND");
  bool window = fs ? !std::strcmp(fs, "window") : (bg && *bg && *bg != '0');
  float pw = m ? float(m->w) : 1024, ph = m ? float(m->h) : 768;  // punten
  SDL_Rect r;
  if (window && SDL_GetDisplayUsableBounds(d, &r)) {
    pw = float(r.w);
    ph = float(r.h) - 28;  // titelbalk van een macOS-venster
  }
  int hh = int(ph * density), ww = int(pw * density);
  hh = std::min(hh, ww * 3 / 4) & ~1;
  *h = hh;
  *w = (hh * 4 / 3) & ~1;
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

}  // namespace vracer

namespace recomp::gen {

void f_00491800_lifted(::recomp::Ctx& c);

// 0x491800 (cdecl kind, value, or): kind 0 zet modus `value` uit de
// modustabel 0x50a9e0 (21 x {w, h, bpp, lores, 0}) in de actieve maat
// 0x50a9c8 (breedte) en 0x50a9cc (hoogte). Voor de 3D-modi 9..13
// (RESOLUTION 3, 4, 7, 8, 9) zetten we daarna onze eigen maat.
void f_00491800(::recomp::Ctx& c) {
  const uint32_t kind = Rd32(c.esp + 4), index = Rd32(c.esp + 8);
  f_00491800_lifted(c);
  if (kind != 0 || index < 9 || index > 13 || vracer::g_render_w < 0) return;
  int w = vracer::g_render_w, h = vracer::g_render_h;
  Wr32(0x50a9c8, uint32_t(w));
  Wr32(0x50a9cc, uint32_t(h));
  std::fprintf(stderr, "vracer: rendermaat %dx%d (modus %u)\n", w, h, index);
}

}  // namespace recomp::gen
