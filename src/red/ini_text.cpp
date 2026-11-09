#include "red/ini_text.h"

#include <dirent.h>

#include <cctype>
#include <cstdlib>
#include <fstream>

namespace red {

std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower((unsigned char)c));
  return s;
}

std::string Trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::vector<std::string> Split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(sep, a);
    out.push_back(Trim(s.substr(a, b == std::string::npos ? std::string::npos : b - a)));
    if (b == std::string::npos) return out;
    a = b + 1;
  }
}

std::string FindNoCase(const std::string& dir, const std::string& rel) {
  std::string cur = dir;
  for (const std::string& part : Split(rel, '/')) {
    DIR* d = opendir(cur.c_str());
    if (!d) return "";
    std::string found;
    while (dirent* e = readdir(d))
      if (Lower(e->d_name) == Lower(part)) {
        found = e->d_name;
        break;
      }
    closedir(d);
    if (found.empty()) return "";
    cur += "/" + found;
  }
  return cur;
}

namespace {

// "value ; comment" -> value
std::string ValueOf(const std::string& text) {
  size_t eq = text.find('=');
  std::string v = text.substr(eq + 1);
  size_t sc = v.find(';');
  if (sc != std::string::npos) v = v.substr(0, sc);
  return Trim(v);
}

}  // namespace

bool IniText::Load(const std::string& path) {
  lines_.clear();
  section_names_.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::string text((std::istreambuf_iterator<char>(f)), {});
  std::string sec;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t nl = text.find('\n', pos);
    std::string l = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? text.size() : nl + 1;
    if (!l.empty() && l.back() == '\r') l.pop_back();
    Line line{l, sec, ""};
    std::string t = Trim(l);
    if (!t.empty() && t[0] == '[') {
      size_t e = t.find(']');
      std::string name = Trim(t.substr(1, e == std::string::npos ? std::string::npos : e - 1));
      sec = Lower(name);
      section_names_.push_back(name);
      line.section = sec;
    } else if (!t.empty() && t[0] != ';') {
      size_t eq = t.find('=');
      if (eq != std::string::npos) line.key = Lower(Trim(t.substr(0, eq)));
    }
    lines_.push_back(line);
  }
  return true;
}

bool IniText::Save(const std::string& path) const {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  for (auto& l : lines_) f << l.text << "\r\n";
  return bool(f);
}

int IniText::Find(const std::string& sec, const std::string& key) const {
  std::string s = Lower(sec), k = Lower(key);
  for (size_t i = 0; i < lines_.size(); i++)
    if (lines_[i].section == s && lines_[i].key == k) return int(i);
  return -1;
}

bool IniText::Has(const std::string& sec, const std::string& key) const { return Find(sec, key) >= 0; }

bool IniText::HasSection(const std::string& sec) const {
  std::string s = Lower(sec);
  for (auto& l : lines_)
    if (l.section == s) return true;
  return false;
}

std::string IniText::Get(const std::string& sec, const std::string& key, const std::string& def) const {
  int i = Find(sec, key);
  return i < 0 ? def : ValueOf(lines_[i].text);
}

int IniText::GetInt(const std::string& sec, const std::string& key, int def) const {
  int i = Find(sec, key);
  if (i < 0) return def;
  std::string v = ValueOf(lines_[i].text);
  char* end;
  long n = std::strtol(v.c_str(), &end, 10);
  return end == v.c_str() ? def : int(n);
}

void IniText::Set(const std::string& sec, const std::string& key, const std::string& value) {
  std::string s = Lower(sec);
  int i = Find(sec, key);
  if (i >= 0) {
    std::string t = lines_[i].text;
    size_t eq = t.find('=');
    size_t sc = t.find(';', eq);
    std::string comment = sc == std::string::npos ? "" : "\t\t" + t.substr(sc);
    std::string k = Trim(t.substr(0, eq));
    lines_[i].text = k + "=" + value + comment;
    return;
  }
  int last = -1;
  for (size_t j = 0; j < lines_.size(); j++)
    if (lines_[j].section == s) last = int(j);
  if (last < 0) {
    if (!lines_.empty() && !Trim(lines_.back().text).empty()) lines_.push_back({"", lines_.back().section, ""});
    lines_.push_back({"[" + sec + "]", s, ""});
    section_names_.push_back(sec);
    last = int(lines_.size()) - 1;
  } else {
    // After the last key or header of the section, before its trailing
    // blank lines and comments.
    while (last > 0 && lines_[last].key.empty() && lines_[last - 1].section == s &&
           Trim(lines_[last].text).rfind('[', 0) != 0)
      last--;
  }
  lines_.insert(lines_.begin() + last + 1, Line{key + "=" + value, s, Lower(key)});
}

std::vector<std::string> IniText::Keys(const std::string& sec) const {
  std::vector<std::string> v;
  std::string s = Lower(sec);
  for (auto& l : lines_)
    if (l.section == s && !l.key.empty()) v.push_back(l.key);
  return v;
}

std::vector<std::string> IniText::Sections() const { return section_names_; }

std::vector<std::pair<std::string, std::string>> IniText::Entries(const std::string& sec) const {
  std::vector<std::pair<std::string, std::string>> v;
  std::string s = Lower(sec);
  for (auto& l : lines_)
    if (l.section == s && !l.key.empty()) v.push_back({Trim(l.text.substr(0, l.text.find('='))), ValueOf(l.text)});
  return v;
}

void IniText::ClearSection(const std::string& sec) {
  std::string s = Lower(sec);
  std::vector<Line> out;
  for (auto& l : lines_)
    if (!(l.section == s && !l.key.empty())) out.push_back(l);
  lines_ = out;
}

}  // namespace red
