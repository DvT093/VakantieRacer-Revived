#include "red/race_mods.h"

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <set>

#include "red/ini_text.h"

namespace red {

namespace {

bool IsDir(const std::string& p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void MakeDirs(const std::string& path) {
  for (size_t i = 1; i <= path.size(); i++)
    if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
}

std::string ReadFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return f ? std::string((std::istreambuf_iterator<char>(f)), {}) : std::string();
}

bool WriteFile(const std::string& p, const std::string& data) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << data;
  return bool(f);
}

bool EndsWith(const std::string& s, const std::string& end) {
  return s.size() >= end.size() && Lower(s.substr(s.size() - end.size())) == end;
}

// The files of a mod folder (not recursive; "local" and hidden files skipped).
std::vector<std::string> ListFiles(const std::string& dir) {
  std::vector<std::string> out;
  DIR* d = opendir(dir.c_str());
  if (!d) return out;
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n.empty() || n[0] == '.' || IsDir(dir + "/" + n)) continue;
    out.push_back(n);
  }
  closedir(d);
  return out;
}

class Layer {
 public:
  explicit Layer(const RaceModConfig& cfg) : cfg_(cfg) {}

  // Path of `name` in the overlay: an existing copy (any case), else the
  // CD's spelling, else `name`.
  std::string OverlayPath(const std::string& name) const {
    std::string p = FindNoCase(cfg_.overlay_dir, name);
    if (!p.empty()) return p;
    std::string cd = FindNoCase(cfg_.game_dir, name);
    return cfg_.overlay_dir + "/" + (cd.empty() ? name : cd.substr(cfg_.game_dir.size() + 1));
  }
  std::string CdPath(const std::string& name) const { return FindNoCase(cfg_.game_dir, name); }

  void Wrote(const std::string& path) { written_.insert(path.substr(cfg_.overlay_dir.size() + 1)); }
  const std::set<std::string>& written() const { return written_; }

 private:
  const RaceModConfig& cfg_;
  std::set<std::string> written_;
};

// Lays the keys of `mod` over `base`; `only_missing` for live files.
// "@from=<section>" first copies every key of another section of the base
// (a new car type that starts as a copy of an existing one).
bool Overlay(IniText& base, const IniText& mod, bool only_missing) {
  bool changed = false;
  for (const std::string& sec : mod.Sections())
    for (auto& [k, v] : mod.Entries(sec)) {
      if (k == "@from") {
        for (auto& [fk, fv] : base.Entries(v))
          if (!only_missing || !base.Has(sec, fk)) {
            base.Set(sec, fk, fv);
            changed = true;
          }
        continue;
      }
      if (only_missing && base.Has(sec, k)) continue;
      if (base.Has(sec, k) && base.Get(sec, k) == v) continue;
      base.Set(sec, k, v);
      changed = true;
    }
  return changed;
}

std::string Join(const std::vector<std::string>& v, char sep) {
  std::string s;
  for (size_t i = 0; i < v.size(); i++) s += (i ? std::string(1, sep) : "") + v[i];
  return s;
}

// The vehicle columns (AnimName, SoundType, Mass, CarType) of row `vehicle`
// into row `car`, and the roof luggage of the vehicle's RefName.
void SwapCarTable(IniText& t, const RaceModConfig& cfg, int car, int vehicle) {
  std::string ref_car, ref_veh;
  for (const std::string& sec : cfg.car_sections) {
    auto c = Split(t.Get(sec, std::to_string(car)), ',');
    auto v = Split(t.Get(sec, std::to_string(vehicle)), ',');
    if (c.size() < 6 || v.size() < 6) continue;
    ref_car = c[0];
    ref_veh = v[0];
    for (int col : {1, 3, 4, 5}) c[col] = v[col];
    t.Set(sec, std::to_string(car), Join(c, ','));
  }
  // [Connected Object Assignments] "<i>=RefName,type,type...", count.
  const char* sec = "Connected Object Assignments";
  if (ref_car.empty() || !t.HasSection(sec)) return;
  std::vector<std::string> rows;
  int n = t.GetInt(sec, "count");
  for (int i = 0; i < n; i++) {
    std::string row = t.Get(sec, std::to_string(i));
    auto f = Split(row, ',');
    if (f.empty() || Lower(f[0]) == Lower(ref_car)) continue;
    rows.push_back(row);
    if (Lower(f[0]) == Lower(ref_veh)) {
      f[0] = ref_car;
      rows.push_back(Join(f, ','));
    }
  }
  t.ClearSection(sec);
  for (size_t i = 0; i < rows.size(); i++) t.Set(sec, std::to_string(i), rows[i]);
  t.Set(sec, "count", std::to_string(rows.size()));
}

}  // namespace

PlayerSetup ApplyRaceMods(const RaceModConfig& cfg) {
  MakeDirs(cfg.overlay_dir);
  Layer L(cfg);
  auto is_live = [&](const std::string& name) {
    for (auto& l : cfg.live_files)
      if (Lower(l) == Lower(name)) return true;
    return false;
  };

  // The player: the live game.ini (the menu wrote it just now).
  PlayerSetup ps;
  {
    IniText gi;
    std::string p = FindNoCase(cfg.overlay_dir, "game.ini");
    gi.Load(p.empty() ? L.CdPath("game.ini") : p);
    ps.car = gi.GetInt("PLAYER", "CAR", -1);
    ps.vehicle = gi.GetInt("PLAYER", "VEHICLE", ps.car);
    if (gi.Has("PLAYER", "TRAILER") && !gi.Get("PLAYER", "TRAILER").empty()) {
      ps.trailer = gi.Get("PLAYER", "TRAILER");
      ps.trailer_set = true;
    }
  }
  bool swap = ps.car >= 0 && ps.vehicle >= 0 && ps.vehicle != ps.car;

  // Mod files by (lower-case) name, in layer order.
  std::map<std::string, std::vector<std::string>> mods;
  std::map<std::string, std::string> spelling;
  std::set<std::string> port;
  for (auto& f : cfg.port_files) port.insert(Lower(f));
  for (const std::string& d : cfg.mod_dirs)
    for (const std::string& n : ListFiles(d)) {
      if (port.count(Lower(n))) continue;
      mods[Lower(n)].push_back(d + "/" + n);
      if (!spelling.count(Lower(n))) spelling[Lower(n)] = n;
    }
  if (swap) {
    mods[Lower(cfg.car_table)];  // swap even without a car table mod
    if (!spelling.count(Lower(cfg.car_table))) spelling[Lower(cfg.car_table)] = cfg.car_table;
  }

  for (auto& [lname, paths] : mods) {
    const std::string& name = spelling[lname];
    std::string out = L.OverlayPath(name);
    if (EndsWith(lname, ".ini")) {
      IniText ini;
      if (is_live(lname)) {
        // Live: the existing copy, only missing keys added.
        std::string cur = FindNoCase(cfg.overlay_dir, name);
        ini.Load(cur.empty() ? L.CdPath(name) : cur);
        bool changed = false;
        for (auto& p : paths) {
          IniText m;
          m.Load(p);
          changed |= Overlay(ini, m, true);
        }
        if (changed || cur.empty()) ini.Save(out);
        continue;
      }
      std::string cd = L.CdPath(name);
      if (!cd.empty()) ini.Load(cd);
      for (auto& p : paths) {
        IniText m;
        m.Load(p);
        Overlay(ini, m, false);
      }
      if (swap && lname == Lower(cfg.car_table)) SwapCarTable(ini, cfg, ps.car, ps.vehicle);
      if (ini.Save(out)) L.Wrote(out);
    } else {
      // Textures, models: the last layer's file.
      if (WriteFile(out, ReadFile(paths.back()))) L.Wrote(out);
    }
  }

  // Per car records (car data, physics): the vehicle's record in the slot.
  if (swap)
    for (auto& rf : cfg.record_files)
      for (size_t league = 0; league < cfg.car_sections.size(); league++) {
        char name[64];
        std::snprintf(name, sizeof name, rf.pattern.c_str(), int(league));
        std::string cd = L.CdPath(name);
        if (cd.empty()) continue;
        std::string data = ReadFile(cd);
        size_t a = size_t(ps.car) * rf.record_size, b = size_t(ps.vehicle) * rf.record_size;
        if (a + rf.record_size > data.size() || b + rf.record_size > data.size()) continue;
        data.replace(a, rf.record_size, data.substr(b, rf.record_size));
        std::string out = L.OverlayPath(name);
        if (WriteFile(out, data)) L.Wrote(out);
      }

  // Remove what the layer wrote last time and not now.
  std::string list = cfg.overlay_dir + "/.race_mods";
  std::ifstream old(list);
  for (std::string rel; std::getline(old, rel);)
    if (!rel.empty() && !L.written().count(rel)) std::remove((cfg.overlay_dir + "/" + rel).c_str());
  std::string now;
  for (auto& rel : L.written()) now += rel + "\n";
  WriteFile(list, now);
  return ps;
}

}  // namespace red
