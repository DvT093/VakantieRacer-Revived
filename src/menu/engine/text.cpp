#include "engine/text.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <cmath>
#include <fstream>
#include <iterator>

namespace davi {

uint32_t Cp1252(unsigned char c) {
  static const uint16_t hi[32] = {0x20AC, 0x81,   0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D,   0x017D, 0x8F,
                                  0x90,   0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D,   0x017E, 0x0178};
  return c >= 0x80 && c < 0xA0 ? hi[c - 0x80] : c;
}

std::string Utf8ToCp1252(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i];
    uint32_t cp;
    int n;
    if (c < 0x80) { cp = c; n = 1; }
    else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; }
    else { cp = c & 0x07; n = 4; }
    for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3f);
    i += n;
    char b = '?';
    if (cp < 0x80 || (cp >= 0xA0 && cp < 0x100)) {
      b = char(cp);
    } else {
      for (int k = 0x80; k < 0xA0; k++)
        if (Cp1252((unsigned char)k) == cp) b = char(k);
    }
    out += b;
  }
  return out;
}

Font::Font() {
  FT_Library lib;
  if (FT_Init_FreeType(&lib) == 0) lib_ = lib;
}

Font::~Font() {
  if (face_) FT_Done_Face((FT_Face)face_);
  if (lib_) FT_Done_FreeType((FT_Library)lib_);
}

bool Font::Load(std::vector<uint8_t> ttf) {
  if (!lib_ || ttf.empty()) return false;
  data_ = std::move(ttf);
  FT_Face face;
  if (FT_New_Memory_Face((FT_Library)lib_, data_.data(), FT_Long(data_.size()), 0, &face) != 0) return false;
  face_ = face;
  return true;
}

bool Font::LoadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  return Load(std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {}));
}

void Font::SetSize(int px) {
  if (px == cur_px_) return;
  FT_Set_Pixel_Sizes((FT_Face)face_, 0, FT_UInt(px));
  cur_px_ = px;
}

int Font::Ascent(int px) {
  if (!face_) return px;
  FT_Face f = (FT_Face)face_;
  auto* os2 = (TT_OS2*)FT_Get_Sfnt_Table(f, FT_SFNT_OS2);
  double a = os2 ? os2->usWinAscent : f->ascender;
  return int(std::lround(a * px / f->units_per_EM));
}

int Font::LineHeight(int px) {
  if (!face_) return px;
  FT_Face f = (FT_Face)face_;
  auto* os2 = (TT_OS2*)FT_Get_Sfnt_Table(f, FT_SFNT_OS2);
  double d = os2 ? os2->usWinDescent : -f->descender;
  return Ascent(px) + int(std::lround(d * px / f->units_per_EM));
}

int Font::Width(const std::string& s, int px) {
  if (!face_) return 0;
  SetSize(px);
  FT_Face f = (FT_Face)face_;
  long w = 0;
  for (unsigned char c : s) {
    if (FT_Load_Char(f, Cp1252(c), FT_LOAD_DEFAULT) == 0) w += f->glyph->advance.x;
  }
  return int((w + 32) >> 6);
}

void Font::DrawLine(SDL_Surface* dst, int x, int y, const std::string& s, int px, SDL_Color c,
                    const SDL_Rect* clip) {
  if (!face_ || !dst) return;
  SetSize(px);
  FT_Face f = (FT_Face)face_;
  SDL_Rect cr = clip ? *clip : SDL_Rect{0, 0, dst->w, dst->h};
  SDL_Rect full{0, 0, dst->w, dst->h};
  SDL_GetRectIntersection(&cr, &full, &cr);
  int base = y + Ascent(px);
  long pen = long(x) << 6;
  if (!SDL_LockSurface(dst)) return;
  const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(dst->format);
  for (unsigned char ch : s) {
    if (FT_Load_Char(f, Cp1252(ch), FT_LOAD_RENDER) != 0) continue;
    FT_GlyphSlot g = f->glyph;
    int gx = int(pen >> 6) + g->bitmap_left;
    int gy = base - g->bitmap_top;
    for (unsigned r = 0; r < g->bitmap.rows; r++) {
      int py = gy + int(r);
      if (py < cr.y || py >= cr.y + cr.h) continue;
      auto* row = (uint32_t*)((uint8_t*)dst->pixels + py * dst->pitch);
      for (unsigned q = 0; q < g->bitmap.width; q++) {
        int pxx = gx + int(q);
        if (pxx < cr.x || pxx >= cr.x + cr.w) continue;
        unsigned a = g->bitmap.buffer[r * g->bitmap.pitch + q];
        if (!a) continue;
        Uint8 dr, dg, db;
        SDL_GetRGB(row[pxx], fmt, nullptr, &dr, &dg, &db);
        auto mix = [a](unsigned d, unsigned s) { return Uint8((s * a + d * (255 - a)) / 255); };
        row[pxx] = SDL_MapRGB(fmt, nullptr, mix(dr, c.r), mix(dg, c.g), mix(db, c.b));
      }
    }
    pen += g->advance.x;
  }
  SDL_UnlockSurface(dst);
}

std::vector<std::string> WrapText(Font& f, const std::string& s, int px, int width, bool wrap) {
  std::vector<std::string> lines;
  std::string para;
  auto flush_para = [&](const std::string& p) {
    if (!wrap) {
      lines.push_back(p);
      return;
    }
    std::string line;
    size_t i = 0;
    while (i <= p.size()) {
      size_t sp = p.find(' ', i);
      std::string word = p.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
      std::string trial = line.empty() ? word : line + " " + word;
      if (!line.empty() && f.Width(trial, px) > width) {
        lines.push_back(line);
        line = word;
      } else {
        line = trial;
      }
      if (sp == std::string::npos) break;
      i = sp + 1;
    }
    lines.push_back(line);
  };
  for (char c : s) {
    if (c == '\r') continue;
    if (c == '\n') {
      flush_para(para);
      para.clear();
    } else {
      para += c;
    }
  }
  flush_para(para);
  return lines;
}

void DrawText(SDL_Surface* dst, const SDL_Rect& r, const std::string& s, const TextStyle& st) {
  if (!st.font || !st.font->Ok() || s.empty()) return;
  auto lines = WrapText(*st.font, s, st.px, r.w, st.wrap);
  int lh = st.font->LineHeight(st.px);
  int total = lh * int(lines.size());
  int y = r.y;
  if (st.valign == VAlign::kCenter) y = r.y + (r.h - total) / 2;
  if (st.valign == VAlign::kBottom) y = r.y + r.h - total;
  for (auto& l : lines) {
    int w = st.font->Width(l, st.px);
    int x = r.x;
    if (st.halign == HAlign::kCenter) x = r.x + (r.w - w) / 2;
    if (st.halign == HAlign::kRight) x = r.x + r.w - w;
    st.font->DrawLine(dst, x, y, l, st.px, st.color, &r);
    y += lh;
  }
}

}  // namespace davi
