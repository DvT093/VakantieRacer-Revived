// Files, bitmaps and sounds for the menu, looked up by name in a list of
// directories. The mod directory comes first, so a file there with the same
// name (any case) replaces the game's original.
#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace davi {

std::vector<uint8_t> ReadFile(const std::string& path);
// Case-insensitive lookup of `rel` ("MenuData/menu.ini", '/' or '\\') below dir.
std::string FindNoCase(const std::string& dir, const std::string& rel);

// A BMP (file bytes) as an XRGB8888 surface, or nullptr.
SDL_Surface* LoadBmp(const std::vector<uint8_t>& bytes);

struct Sound {
  SDL_AudioSpec spec{};
  std::vector<uint8_t> pcm;
  bool Ok() const { return !pcm.empty(); }
};
Sound LoadWav(const std::vector<uint8_t>& bytes);

class Assets {
 public:
  // Directories searched in order of priority (first = wins).
  void AddDir(const std::string& dir) { dirs_.push_back(dir); }
  std::string Find(const std::string& name) const;  // "" if not found
  // "MAINSCRE" or "mainscre.bmp"; cached, owned by Assets.
  SDL_Surface* Bitmap(const std::string& name);
  const Sound* Wav(const std::string& name);
  const std::vector<std::string>& Dirs() const { return dirs_; }

 private:
  std::vector<std::string> dirs_;
  std::map<std::string, SDL_Surface*> bmps_;
  std::map<std::string, std::unique_ptr<Sound>> wavs_;
};

// Simple mixer on SDL3 audio streams: fire-and-forget sounds, one voice
// channel (a new voice line stops the previous one) and looping music.
class Audio {
 public:
  bool Init();
  void Shutdown();
  void Play(const Sound* s, float gain = 1.0f);
  void PlayVoice(const Sound* s, float gain = 1.0f);
  bool VoicePlaying() const;
  void StopVoice();
  void PlayMusic(const Sound* s, float gain);
  void SetMusicGain(float gain);
  void StopMusic();
  void Pause(bool paused);  // while the race runs
  // A raw stream on the device (owned by the caller), e.g. for video sound.
  SDL_AudioStream* OpenStream(const SDL_AudioSpec& spec, float gain);
  void Update();            // refills the music loop, frees finished sounds

 private:
  SDL_AudioDeviceID dev_ = 0;
  std::vector<SDL_AudioStream*> sfx_;
  SDL_AudioStream* voice_ = nullptr;
  SDL_AudioStream* music_ = nullptr;
  const Sound* music_snd_ = nullptr;
};

}  // namespace davi
