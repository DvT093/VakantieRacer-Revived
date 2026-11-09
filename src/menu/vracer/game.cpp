#include "vracer/game.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <typeinfo>

namespace vr {

using davi::Split;

VrScreen::VrScreen(Game& g, const std::string& cls) : davi::Screen(g.app, cls), game(g) {
  // btnHelp: only when MenuData\VRacer.hlp exists (Data+0xbc); it does not on the CD.
  if (davi::Widget* h = form.Get("btnHelp")) {
    h->visible = !g.help_file.empty();
    h->caption = g.Str(18);
  }
  // Bulbs along the rail (TGameMenuScreen, not on the intro screens).
  if (davi::Widget* b = form.Get("imgBulb")) {
    bulb_ = b->picture;
    b->visible = false;
    if (cls == "TfrmIntro" || cls == "TfrmTrackIntro") bulb_ = nullptr;
  }
}

void VrScreen::Update(double dt) {
  bulb_t_ += dt;
  if (bulb_t_ >= 0.2) {  // 5 frames per second
    bulb_t_ -= 0.2;
    bulb_frame_ ^= 1;
  }
  Tick(dt);
}

void VrScreen::PaintOver(SDL_Surface* canvas, int ox, int oy) {
  if (bulb_ && bulb_->w >= 2) {
    // 26 bulbs at (182 + 17i, 96); two frames side by side, neighbours alternate.
    int fw = bulb_->w / 2;
    for (int i = 0; i < 26; i++) {
      SDL_Rect src{((i + bulb_frame_) & 1) * fw, 0, fw, bulb_->h};
      SDL_Rect dst{ox + 182 + 17 * i, oy + 96, fw, bulb_->h};
      davi::Blit(canvas, bulb_, &src, dst);
    }
  }
  Paint(canvas, ox, oy);
}

namespace {

std::mt19937& Rng() {
  static std::mt19937 r{std::random_device{}()};
  return r;
}

int ToInt(const std::string& s, int def = 0) {
  char* e;
  long v = std::strtol(s.c_str(), &e, 10);
  return e == s.c_str() ? def : int(v);
}

}  // namespace

// Caption rectangles of the TDaviGraphicDef handlers (TData and per form):
// Rect(10, top, Width - 10, Height - 5). From Menu.exe 0x469540 (defBtn,
// top 38), 0x468550 (defFriendlyMatch: OK/Back/Main menu, top 32),
// FormMainScreen 0x4859d8 (Quick Race, top 145) / 0x485aa0 (Vakantie Race, top 205).
void RegisterSkin(davi::Skin& skin) {
  auto rect = [](int top) {
    return [top](const davi::Widget& w) { return SDL_Rect{10, top, w.r.w - 20, w.r.h - 5 - top}; };
  };
  skin.caption_rects["defBtnGetCaptionRect"] = rect(38);
  skin.caption_rects["defFriendlyMatchGetCaptionRect"] = rect(32);
  skin.caption_rects["defQRBtnGetCaptionRect"] = rect(145);
  skin.caption_rects["defVRBtnGetCaptionRect"] = rect(205);
  skin.caption_rects["defEasyBtnGetCaptionRect"] = rect(100);    // FormDifficulty
  skin.caption_rects["defRepairBtnGetCaptionRect"] = rect(36);   // FormGarage
  skin.caption_rects["defTurboBtnGetCaptionRect"] = rect(36);
  skin.caption_rects["defMotor1BtnGetCaptionRect"] = rect(44);
  skin.caption_rects["def0GetCaptionRect"] = rect(85);           // FormPlayer
  // TGameMenuScreen.FormCreate: background = copy(ClassName, 5, 8) + ".bmp".
  davi::DelphiExe* exe = skin.exe;
  skin.background_name = [exe](const std::string& cls) -> std::string {
    for (std::string p = cls; !p.empty(); p = exe->ParentClass(p))
      if (p == "TGameMenuScreen") return cls.size() > 4 ? cls.substr(4, 8) : "";
    return "";
  };
}

Game::Game(davi::App& a, red::Install install, std::vector<std::string> race_argv)
    : app(a), assets(*a.skin.assets), launcher(std::move(install), std::move(race_argv)) {}

bool Game::Load() {
  const red::Install& in = launcher.install();
  help_file = davi::FindNoCase(in.game_dir, "MenuData/VRacer.hlp");
  stat.AddLayer(in.ReadPath("MenuData/static.ini"));
  menu.AddLayer(in.ReadPath("MenuData/menu.ini"));
  // Mod layer: mods/menu/static.ini, mods/menu/menu.ini override keys.
  for (auto& d : assets.Dirs()) {
    if (d.find("MenuData") != std::string::npos) continue;
    stat.AddLayer(d + "/static.ini");
    menu.AddLayer(d + "/menu.ini");
  }

  for (int i = 0;; i++) {
    std::string s = "char" + std::to_string(i);
    if (!stat.HasSection(s)) break;
    Character c;
    c.index = i;
    c.section = s;
    auto g = [&](const char* k) { return stat.Get(s, k); };
    c.name = g("name");
    c.bitmap = g("bitmap");
    c.carbitmap = g("carbitmap");
    c.qrbmp = g("qrbmp");
    c.wofbmp = g("wofbmp");
    c.resultsbmp = g("resultsbmp");
    c.hostwinbmp = g("hostwinbmp");
    c.hostlosebmp = g("hostlosebmp");
    c.hostqualbmp = g("hostqualbmp");
    c.introwav = g("introwav");
    c.outrowav = g("outrowav");
    c.chosenwav = g("chosenwav");
    c.rndwinprizewav = g("RndWinPrizeWav");
    c.rndwinnothingwav = g("RndWinNothingWav");
    c.wintext = g("WINTEXT");
    c.winracetext = g("winracetext");
    c.story = g("story");
    c.video = g("video");
    c.indexingame = stat.GetInt(s, "indexingame", i);
    c.speed = stat.GetInt(s, "speed");
    c.accell = stat.GetInt(s, "accell");
    c.handling = stat.GetInt(s, "handling");
    c.brakes = stat.GetInt(s, "brakes");
    chars.push_back(c);
  }
  // Port: the autoselector's cars and the trailers (mods/menu/static.ini).
  for (int i = 0; i < 32; i++) {
    std::string s = "car" + std::to_string(i);
    if (!stat.HasSection(s)) continue;
    Car c;
    c.index = i;
    c.name = stat.Get(s, "name");
    c.carbitmap = stat.Get(s, "carbitmap");
    c.speed = stat.GetInt(s, "speed");
    c.accell = stat.GetInt(s, "accell");
    c.handling = stat.GetInt(s, "handling");
    c.brakes = stat.GetInt(s, "brakes");
    c.trailer = stat.Get(s, "trailer");
    c.bonus = stat.GetInt(s, "bonus") != 0;
    if (int(cars.size()) <= i) cars.resize(i + 1);
    cars[i] = c;
  }
  for (size_t i = 0; i < cars.size(); i++)
    if (cars[i].name.empty()) cars[i].index = -1;
  int ntrailers = stat.GetInt("TRAILERS", "COUNT", 0);
  for (int i = 0; i < ntrailers; i++) {
    auto v = Split(stat.Get("TRAILERS", std::to_string(i)), ',');
    v.resize(3);
    trailers.push_back({v[0], v[1], v[2]});
  }
  if (trailers.empty()) trailers.push_back({"NONE", "geen", ""});

  int ntracks = stat.GetInt("GENERAL", "TRACKCOUNT", 0);
  for (int i = 0; i < ntracks || stat.HasSection("TRACK" + std::to_string(i)); i++) {
    std::string s = "TRACK" + std::to_string(i);
    if (!stat.HasSection(s)) break;
    Track t;
    t.index = i;
    t.name = stat.Get(s, "NAME");
    t.type = stat.Get(s, "TYPE");
    t.length = stat.Get(s, "LENGTH");
    t.bitmap = stat.Get(s, "BITMAP");
    t.story = stat.Get(s, "STORY");
    t.wintext = stat.Get(s, "WINTEXT");
    t.qualifytext = stat.Get(s, "QUALIFYTEXT");
    t.losetext = stat.Get(s, "LOSETEXT");
    t.introwav = stat.Get(s, "INTROWAV");
    t.nrlaps = stat.GetInt(s, "NRLAPS", 1);
    t.indexingame = stat.GetInt(s, "INDEXINGAME", i);
    for (auto& q : Split(stat.Get(s, "QUALIFYPOSITION", "0,0,0"), ',')) t.qualify.push_back(ToInt(q));
    tracks.push_back(t);
  }
  for (int i = 0; stat.Has("TUNEUPS", "SPECS" + std::to_string(i)); i++) {
    auto v = Split(stat.Get("TUNEUPS", "SPECS" + std::to_string(i)), ',');
    TuneUp t;
    if (v.size() >= 6) {
      t.name = v[0];
      t.price = ToInt(v[1]);
      t.dspeed = ToInt(v[2]);
      t.dacc = ToInt(v[3]);
      t.dbrakes = ToInt(v[4]);
      t.dhandl = ToInt(v[5]);
    }
    t.text = stat.Get("TUNEUPS", "TEXT" + std::to_string(i));
    tuneups.push_back(t);
  }
  int np = stat.GetInt("wofprizes", "count");
  for (int i = 0; i < np; i++) {
    std::string n = std::to_string(i);
    prizes.push_back({stat.GetInt("wofprizes", n + "istuneup") != 0, stat.GetInt("wofprizes", n + "value"),
                      stat.Get("wofprizes", n + "wintext"), stat.Get("wofprizes", n + "imagefile")});
  }
  for (size_t i = 0; i < tracks.size(); i++) {
    std::string n = std::to_string(i);
    records.push_back({menu.GetInt("HIGHSCORES", n), menu.Get("HIGHSCORES", "name" + n)});
  }

  // Players (native file; the original used binary save games).
  players_path_ = in.user_dir + "/menu_players.ini";
  davi::IniFile pf;
  if (pf.Load(players_path_)) {
    int n = pf.GetInt("GENERAL", "count");
    for (int i = 0; i < n; i++) {
      Player p;
      PlayerIo(pf, "player" + std::to_string(i), p, false);
      players.push_back(p);
    }
    current_player = pf.GetInt("GENERAL", "current", -1);
    if (current_player >= int(players.size())) current_player = -1;
  }
  return !chars.empty() && !tracks.empty();
}

// Reads (save=false) or writes one player section of menu_players.ini.
void Game::PlayerIo(davi::IniFile& f, const std::string& s, Player& p, bool save) {
  auto num = [&](const char* k, int& v) {
    if (save) f.SetInt(s, k, v);
    else v = f.GetInt(s, k, v);
  };
  auto flag = [&](const char* k, bool& v) {
    int i = v;
    num(k, i);
    v = i != 0;
  };
  if (save) f.Set(s, "name", p.name);
  else p.name = f.Get(s, "name");
  num("difficulty", p.difficulty);
  flag("showstartinfo", p.show_start_info);
  num("musicvolume", p.music_volume);
  num("fxvolume", p.fx_volume);
  num("voicesvolume", p.voices_volume);
  num("resolution", p.resolution);
  num("device", p.device);
  static const char* keys[7] = {"up", "down", "left", "right", "extra1", "extra2", "extra3"};
  for (int k = 0; k < 7; k++) num(keys[k], p.keys[k]);
  num("joysensitivity", p.joy_sensitivity);
  num("wheelsensitivity", p.wheel_sensitivity);
  num("gamma", p.gamma);
  flag("alphablending", p.alpha);
  flag("bilinearfiltering", p.bilinear);
  flag("waitvbl", p.waitvbl);
  num("qrchar", p.qr_char);
  num("qrtrack", p.qr_track);
  num("qrcar", p.qr_car);
  num("qrtrailer", p.qr_trailer);
  flag("unlockall", p.unlock_all);
  num("currentchar", p.current_char);
  num("currenttrack", p.current_track);
  // Per character: tracktodrive,money,won,damage,turbo,motor,banden,abs,
  // versnellingsbak, then (port) car and trailer (-1 = standard).
  size_t nres = save ? p.results.size() : chars.size();
  for (size_t c = 0; c < nres; c++) {
    std::string k = "char" + std::to_string(c);
    CharResult& r = p.Res(int(c));
    if (save) {
      std::string v = std::to_string(r.tracktodrive) + "," + std::to_string(r.money) + "," +
                      std::to_string(r.won) + "," + std::to_string(r.damage) + "," + std::to_string(r.turbo);
      for (bool t : r.tuneup) v += t ? ",1" : ",0";
      v += "," + std::to_string(r.car) + "," + std::to_string(r.trailer);
      f.Set(s, k, v);
    } else if (f.Has(s, k)) {
      auto v = Split(f.Get(s, k), ',');
      v.resize(9, "0");
      v.resize(11, "-1");
      r.tracktodrive = ToInt(v[0]);
      r.money = ToInt(v[1]);
      r.won = ToInt(v[2]) != 0;
      r.damage = ToInt(v[3]);
      r.turbo = ToInt(v[4]);
      for (int t = 0; t < 4; t++) r.tuneup[t] = ToInt(v[5 + t]) != 0;
      r.car = ToInt(v[9], -1);
      r.trailer = ToInt(v[10], -1);
    }
  }
}

// CreateNewPlayer: settings from static.ini [DEFAULTSETTINGS].
void Game::NewPlayer(const std::string& name) {
  Player p;
  p.name = name;
  const char* d = "DEFAULTSETTINGS";
  p.difficulty = stat.GetInt(d, "DIFFICULTY", 1);
  p.show_start_info = stat.GetInt(d, "SHOWSTARTINFO", 1) != 0;
  p.music_volume = stat.GetInt(d, "MUSICON", 1) ? stat.GetInt(d, "MUSICVOLUME", 35) : 0;
  p.fx_volume = stat.GetInt(d, "FXON", 1) ? stat.GetInt(d, "FXVOLUME", 75) : 0;
  p.voices_volume = stat.GetInt(d, "VOICESON", 1) ? stat.GetInt(d, "VOICESVOLUME", 75) : 0;
  p.resolution = stat.GetInt(d, "RESOLUTION", 3);
  p.device = stat.GetInt(d, "CONTROLTYPE", 1);
  static const char* keys[7] = {"UP", "DOWN", "LEFT", "RIGHT", "EXTRA1", "EXTRA2", "EXTRA3"};
  for (int k = 0; k < 7; k++) p.keys[k] = stat.GetInt(d, keys[k], p.keys[k]);
  p.joy_sensitivity = stat.GetInt(d, "JOYSENSITIVITY", 50);
  p.wheel_sensitivity = stat.GetInt(d, "WHEELSENSITIVITY", 50);
  p.alpha = stat.GetInt(d, "AlphaBlending", 1) != 0;
  p.bilinear = stat.GetInt(d, "BiliniarFiltering", 1) != 0;
  p.waitvbl = stat.GetInt(d, "WaitVbl", 0) != 0;
  p.gamma = stat.GetInt(d, "Gamma", 0);
  p.results.resize(chars.size());
  players.push_back(p);
  current_player = int(players.size()) - 1;
  SavePlayers();
}

int Game::CharByIndexInGame(int car) const {
  for (auto& c : chars)
    if (c.indexingame == car) return c.index;
  return -1;
}

const Car* Game::CarInfo(int car) const {
  return car >= 0 && car < int(cars.size()) && cars[car].index == car ? &cars[car] : nullptr;
}

bool Game::BonusUnlocked() {
  Player* p = Cur();
  if (!p) return false;
  if (p->unlock_all) return true;
  for (auto& c : chars)
    if (!p->Res(c.index).won) return false;
  return true;
}

bool Game::CarAvailable(int car) {
  const Car* c = CarInfo(car);
  return c && (!c->bonus || BonusUnlocked());
}

int Game::TrailerIndex(const std::string& ref) const {
  for (size_t i = 0; i < trailers.size(); i++)
    if (davi::Lower(trailers[i].ref) == davi::Lower(ref)) return int(i);
  return 0;
}

SDL_Surface* Game::CarPicture(int car, int w, int h) {
  const Car* c = CarInfo(car);
  if (!c) return nullptr;
  std::string key = c->carbitmap + "@" + std::to_string(w) + "x" + std::to_string(h);
  auto it = scaled_.find(key);
  if (it != scaled_.end()) return it->second;
  SDL_Surface* src = assets.Bitmap(c->carbitmap);
  SDL_Surface* out = nullptr;
  if (src) {
    out = SDL_CreateSurface(w, h, src->format);
    if (out) SDL_BlitSurfaceScaled(src, nullptr, out, nullptr, SDL_SCALEMODE_LINEAR);
    davi::DerivePicture(out, src, nullptr, nullptr);  // upgraded menu: drawn from src's HD picture
  }
  scaled_[key] = out;
  return out;
}

SDL_Surface* Game::TrailerPicture(int trailer) {
  if (trailer < 0 || trailer >= int(trailers.size()) || trailers[trailer].bitmap.empty()) return nullptr;
  return assets.Bitmap(trailers[trailer].bitmap);
}

void Game::SavePlayers() {
  davi::IniFile pf;
  pf.SetInt("GENERAL", "count", int(players.size()));
  pf.SetInt("GENERAL", "current", current_player);
  for (size_t i = 0; i < players.size(); i++) PlayerIo(pf, "player" + std::to_string(i), players[i], true);
  pf.Save(players_path_);
}

void Game::SaveRecords() {
  const red::Install& in = launcher.install();
  davi::IniFile mi;
  mi.Load(in.ReadPath("MenuData/menu.ini"));
  for (size_t i = 0; i < records.size(); i++) {
    std::string n = std::to_string(i);
    mi.SetInt("HIGHSCORES", n, records[i].time_ms);
    mi.Set("HIGHSCORES", "name" + n, records[i].name);
  }
  mi.Save(in.WritePath("MenuData/menu.ini"));
}

std::string Game::Str(int n) const { return menu.Get("MENUSTRINGS", std::to_string(n)); }

std::string Game::Ordinal(int pos1) const {
  std::string s = menu.Get("ordinalnumbers", std::to_string(pos1));
  return s.empty() ? std::to_string(pos1) + "e" : s;
}

std::string Game::FormatTime(int ms) const {
  // [FORMAT] TIME=%d:%.2d:%.2d -> minutes, seconds, hundredths.
  std::string f = menu.Get("FORMAT", "TIME", "%d:%.2d:%.2d");
  char buf[64];
  std::snprintf(buf, sizeof buf, f.c_str(), ms / 60000, (ms / 1000) % 60, (ms / 10) % 100);
  return buf;
}

static float Gain(int vol) { return std::max(0, std::min(100, vol)) / 100.0f; }

void Game::Voice(const std::string& base, int n) {
  std::string name = base;
  if (n > 0) name += std::to_string(std::uniform_int_distribution<int>(0, n - 1)(Rng()));
  Player* p = Cur();
  app.audio.PlayVoice(assets.Wav(name), Gain(p ? p->voices_volume : 75));
}

void Game::Sound(const std::string& name) {
  Player* p = Cur();
  const davi::Sound* s = nullptr;
  if (name.rfind("__", 0) == 0) {
    auto it = embedded.find(name.substr(2));
    if (it != embedded.end()) s = &it->second;
  } else {
    s = assets.Wav(name);
  }
  app.audio.Play(s, Gain(p ? p->fx_volume : 75));
}

void Game::Goto(davi::Screen* s) {
  if (app.Current() && app.Current() != s) prev_[s] = app.Current();
  app.Show(s);
}

void Game::Back() {
  auto it = prev_.find(app.Current());
  if (it != prev_.end()) app.Show(it->second);
}

void Game::StartMusic() {
  Player* p = Cur();
  app.audio.PlayMusic(assets.Wav("menumsc"), Gain(p ? p->music_volume : 35));
}

}  // namespace vr
