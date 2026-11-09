// RED engine: .ini files as the race reads them (GetPrivateProfileString),
// edited as text. Sections and keys are case-insensitive; every line that
// is not set (comments, tables, order) stays exactly as it was, so a file
// written back is the original plus the changed keys.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace red {

std::string Lower(std::string s);
std::string Trim(const std::string& s);
std::vector<std::string> Split(const std::string& s, char sep);
// `rel` below `dir`, matching every path component case-insensitively;
// "" if it does not exist.
std::string FindNoCase(const std::string& dir, const std::string& rel);

class IniText {
 public:
  bool Load(const std::string& path);  // false if missing (stays empty)
  bool Save(const std::string& path) const;

  bool Has(const std::string& sec, const std::string& key) const;
  bool HasSection(const std::string& sec) const;
  // The value without a trailing "; comment".
  std::string Get(const std::string& sec, const std::string& key, const std::string& def = "") const;
  int GetInt(const std::string& sec, const std::string& key, int def = 0) const;
  // Replaces the value (keeping the key's spelling and a trailing comment)
  // or adds the key at the end of the section (or a new section).
  void Set(const std::string& sec, const std::string& key, const std::string& value);
  // Keys of a section in file order (lower case) and all section names.
  std::vector<std::string> Keys(const std::string& sec) const;
  std::vector<std::string> Sections() const;
  // (key as written, value) of a section in file order.
  std::vector<std::pair<std::string, std::string>> Entries(const std::string& sec) const;
  // Removes every key line of a section (comments stay).
  void ClearSection(const std::string& sec);

 private:
  struct Line {
    std::string text;     // the line without its newline
    std::string section;  // lower case
    std::string key;      // lower case, "" if not a key line
  };
  int Find(const std::string& sec, const std::string& key) const;
  std::vector<Line> lines_;
  std::vector<std::string> section_names_;  // original spelling
};

}  // namespace red
