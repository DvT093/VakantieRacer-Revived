// Starting a race and taking in its results, as Menu.exe did it:
// TData.WriteToInis before the race, TfrmMain.FillRaceResultsQR /
// FillRaceResultsVR (0x48e17c / 0x48e374) after it.
#include <algorithm>

#include "vracer/screens.h"

namespace vr {

namespace {

// The extra Quick Race entry after the last character: car 5 (Ferrari).
constexpr int kBonusCar = 5;

// WriteToInis: game.ini keys for the race. Mods can add their own keys
// through Game::launcher.before_race.
void WriteRaceKeys(Game& g, int char_index, int track_index) {
  Player& p = *g.Cur();
  red::RaceLauncher& L = g.launcher;
  // [INPUT]: joystick (3) and wheel (4) both run as DEVICE=3.
  int dev = p.device == 4 ? 3 : p.device;
  L.SetInt("INPUT", "DEVICE", dev);
  L.SetInt("INPUT", "SENSITIVITY", p.device == 3 ? p.joy_sensitivity : p.device == 4 ? p.wheel_sensitivity : 50);
  static const char* keys[7] = {"UP", "DOWN", "LEFT", "RIGHT", "EXTRA1", "EXTRA2", "EXTRA3"};
  for (int k = 0; k < 7; k++) L.SetInt("INPUT", keys[k], p.keys[k]);
  const Track& t = g.tracks[track_index];
  L.SetInt("ETAPPE", "ETAPPE", t.indexingame);
  L.SetInt("OPTIONS", "NRLAPS", t.nrlaps);
  L.SetInt("LEAGUE", "LEAGUE", p.difficulty);
  L.SetInt("GFX OPTIONS", "gamma", p.gamma);
  L.SetInt("GFX OPTIONS", "alpha_blending", p.alpha);
  L.SetInt("OPTIONS", "waitvbl", p.waitvbl);
  L.SetInt("OPTIONS", "filter", p.bilinear ? 2 : 0);
  bool bonus = char_index < 0 || char_index >= int(g.chars.size());
  int car = bonus ? kBonusCar : g.chars[char_index].indexingame;
  if (g.mode == 1) {
    L.SetInt("PLAYER", "MONEY", 0);
    L.SetInt("PLAYER", "CAR", car);
    L.SetInt("PLAYER", "DAMAGE", 0);
    L.SetInt("PLAYER", "TURBO_AMOUNT", 100);
    for (const char* k : {"MOTOR", "BANDEN", "ABS", "VERSNELLINGSBAK"}) L.SetInt("PLAYER", k, 1);
    L.SetInt("PLAYER", "POSITION", 4);
  } else {
    CharResult& r = p.Res(char_index);
    L.SetInt("PLAYER", "MONEY", r.money);
    L.SetInt("PLAYER", "CAR", car);
    L.SetInt("PLAYER", "DAMAGE", r.damage);
    const char* tk[4] = {"MOTOR", "BANDEN", "ABS", "VERSNELLINGSBAK"};
    for (int k = 0; k < 4; k++) L.SetInt("PLAYER", tk[k], r.tuneup[k]);
    L.SetInt("PLAYER", "TURBO_AMOUNT", r.turbo);
  }
}

// 0x468f1c: the four computer racers of [RACERS] and the player, by time.
void Rank(Game& g, int player_char, const red::RaceResult& r) {
  g.ranking.clear();
  for (size_t i = 0; i < r.racers.size() && i < 4; i++)
    g.ranking.push_back({g.CharByIndexInGame(r.racers[i].car), r.racers[i].time_ms});
  g.ranking.push_back({player_char, r.tracktime});
  size_t player_entry = g.ranking.size() - 1;
  std::vector<size_t> order(g.ranking.size());
  for (size_t i = 0; i < order.size(); i++) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return g.ranking[a].time_ms < g.ranking[b].time_ms;
  });
  std::vector<Ranked> sorted;
  for (size_t i = 0; i < order.size(); i++) {
    sorted.push_back(g.ranking[order[i]]);
    if (order[i] == player_entry) g.playerpos = int(i);
  }
  g.ranking = sorted;
}

// A lap faster than the track record (menu.ini [HIGHSCORES]) replaces it;
// the name is asked on the results screen.
void BestLaps(Game& g, int track) {
  davi::IniFile gi = g.launcher.LoadGameIni();
  Record& rec = g.records[track];
  for (int lap = 1; lap <= g.tracks[track].nrlaps; lap++) {
    int t = gi.GetInt("PLAYER", "LAP_" + std::to_string(lap) + "_TIME");
    if (t > 0 && t < rec.time_ms) {
      g.rrflags |= kBestLap;
      rec.time_ms = t;
    }
  }
}

}  // namespace

void StartRace(Game& g, int char_index, int track_index, std::function<void()> after) {
  auto go = [&g, char_index, track_index, after]() {
    WriteRaceKeys(g, char_index, track_index);
    g.SavePlayers();
    g.last = g.launcher.Run(g.app);
    g.prevstate = g.mode;
    g.StartMusic();
    TakeResults(g, char_index, track_index);
    after();
  };
  // Garage.btnOk / QuickRace.btnOk: the key overview first when it is on
  // and the player drives with the keyboard.
  Player* p = g.Cur();
  if (p && p->show_start_info && p->device == 1) {
    auto* si = g.Screen<StartInfoPopup>();
    g.app.ShowModal(si, [go](int res) {
      if (res == 1) go();
    });
  } else {
    go();
  }
}

void TakeResults(Game& g, int char_index, int track_index) {
  const red::RaceResult& r = g.last;
  g.rrflags = 0;
  g.playerpos = 0;
  g.ranking.clear();
  if (!r.finished) return;
  Player& p = *g.Cur();
  if (g.mode == 1) {
    bool bonus = char_index >= int(g.chars.size());
    Rank(g, bonus ? -1 : char_index, r);
    BestLaps(g, track_index);
    return;
  }
  CharResult& cr = p.Res(char_index);
  for (bool& t : cr.tuneup) t = false;  // tune-ups last one race
  davi::IniFile gi = g.launcher.LoadGameIni();
  cr.money = gi.GetInt("PLAYER", "MONEY", cr.money);
  cr.turbo = r.turbo_amount;
  cr.damage = r.damage;
  Rank(g, char_index, r);
  BestLaps(g, track_index);
  int ntracks = int(g.tracks.size());
  if (g.playerpos == 0) {
    if (track_index == ntracks - 1) {
      g.rrflags |= kWinRace;
      cr.won = true;
      bool all = true;
      for (auto& c : g.chars) all = all && p.Res(c.index).won;
      if (cr.tracktodrive == track_index && all) g.rrflags |= kWinAll;
    } else {
      g.rrflags |= kFortune;
    }
  }
  const Track& t = g.tracks[track_index];
  int q = p.difficulty < int(t.qualify.size()) ? t.qualify[p.difficulty] : 0;
  if (g.playerpos <= q) {
    g.rrflags |= kQualify;
    cr.tracktodrive = track_index + 1;
    if (cr.tracktodrive == ntracks) cr.tracktodrive = 0;
  }
  g.SavePlayers();
}

}  // namespace vr
