// TfrmOptions (OPTIONS.bmp) and its popups TfrmKeySettings (POP_KEYS),
// TfrmJoystickSettings (POP_JOYS) and TfrmWheelSettings (POP_WHEE).
#include "vracer/screens.h"

namespace vr {

namespace {

// Windows virtual-key code of an SDL key (the keys menu.ini [CONTROLKEYS]
// offers), 0 if none.
int VirtualKey(SDL_Keycode k) {
  if (k >= SDLK_A && k <= SDLK_Z) return 'A' + int(k - SDLK_A);
  if (k >= SDLK_0 && k <= SDLK_9) return '0' + int(k - SDLK_0);
  if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return 97 + int(k - SDLK_KP_1);
  switch (k) {
    case SDLK_LEFT: return 37;
    case SDLK_UP: return 38;
    case SDLK_RIGHT: return 39;
    case SDLK_DOWN: return 40;
    case SDLK_INSERT: return 45;
    case SDLK_DELETE: return 46;
    case SDLK_HOME: return 36;
    case SDLK_END: return 35;
    case SDLK_PAGEUP: return 33;
    case SDLK_PAGEDOWN: return 34;
    case SDLK_KP_0: return 96;
    case SDLK_KP_MULTIPLY: return 106;
    case SDLK_KP_PLUS: return 107;
    case SDLK_KP_MINUS: return 109;
    case SDLK_KP_PERIOD: return 110;
    case SDLK_KP_DIVIDE: return 111;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 16;
    // Port: Cmd is the race's Ctrl (VK 17, see main.cpp key_aliases):
    // macOS keeps Ctrl+arrow for itself, so Ctrl is no key here.
    case SDLK_LGUI: case SDLK_RGUI: return 17;
    case SDLK_LALT: case SDLK_RALT: return 18;
    case SDLK_SPACE: return 32;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 13;
    case SDLK_BACKSPACE: return 8;
    case SDLK_TAB: return 9;
    case SDLK_COMMA: return 188;
    case SDLK_PERIOD: return 190;
    case SDLK_SLASH: return 191;
    case SDLK_SEMICOLON: return 186;
    case SDLK_APOSTROPHE: return 222;
    case SDLK_LEFTBRACKET: return 219;
    case SDLK_RIGHTBRACKET: return 221;
    case SDLK_BACKSLASH: return 220;
    default: return 0;
  }
}

// menu.ini [CONTROLKEYS]: "<n>=<vk>,<name>"; lines starting with '*' are off.
std::string KeyName(Game& g, int vk, bool* allowed = nullptr) {
  if (allowed) *allowed = false;
  for (auto& k : g.menu.Keys("CONTROLKEYS")) {
    if (k.empty() || k[0] == '*' || k == "count") continue;
    auto v = davi::Split(g.menu.Get("CONTROLKEYS", k), ',');
    if (v.size() >= 2 && std::atoi(v[0].c_str()) == vk) {
      if (allowed) *allowed = vk != 0;
      return v[1];
    }
  }
  return "";
}

float Gain(int v) { return std::max(0, std::min(100, v)) / 100.0f; }

}  // namespace

std::string ControlKeyName(Game& g, int vk) { return KeyName(g, vk); }

// ----------------------------------------------------------------- Options

OptionsScreen::OptionsScreen(Game& g) : VrScreen(g, "TfrmOptions") {
  // FormCreate (0x47d868).
  form.SetPicture("imgDisableLow", g.assets.Bitmap("OPT_DLOW"));
  form.SetPicture("imgDisableHigh", g.assets.Bitmap("OPT_DHIG"));
  form.SetPicture("imgVoiceOn", g.assets.Bitmap("OPT_STE1"));
  form.SetPicture("imgMusicOn", g.assets.Bitmap("OPT_MUZ1"));
  form.SetPicture("imgSoundOn", g.assets.Bitmap("OPT_SFX1"));
  if (auto opt = g.app.skin.exe->Form("TfrmOptions"))
    if (davi::Component* w = opt->Find("wavIrritatingAsHell"))
      sample_ = davi::LoadWav(davi::SizedBytes(*w->Prop("WavFileData")));
}

// CopySettingsToControls (0x47cc64) on a copy of the player's settings.
void OptionsScreen::OnShow() {
  Player* p = game.Cur();
  temp_ = p ? *p : Player{};
  int res = temp_.resolution;
  bool d3 = res == 3 || res == 4 || res == 6;
  form.W("chkVideo3d").checked = d3;
  form.W("optVideoLow").checked = res == 0 || res == 6;
  form.W("optVideoMedium").checked = res == 1 || res == 3;
  form.W("optVideoHigh").checked = !(res == 0 || res == 6 || res == 1 || res == 3);
  Video();
  form.W("chkStartInfoOn").checked = temp_.show_start_info;
  form.W("chkUnlockAll").checked = temp_.unlock_all;  // port: Alles vrij
  form.W("chkTrailerChoice").checked = temp_.trailer_choice;  // port: Caravan kiezen
  form.W("sldrMusic").pos = temp_.music_volume;
  form.W("sldrSound").pos = temp_.fx_volume;
  form.W("sldrVoice").pos = temp_.voices_volume;
  form.SetVisible("imgMusicOn", temp_.music_volume > 0);
  form.SetVisible("imgSoundOn", temp_.fx_volume > 0);
  form.SetVisible("imgVoiceOn", temp_.voices_volume > 0);
  Control(temp_.device);
  gfx_ = vracer::LoadGfxSettings();
  game.app.SetFocus(form.Get("btnOk"));
}

// Video options: Low only without, High only with the 3D card (0x47dad8,
// 0x47db6c).
void OptionsScreen::Video() {
  bool d3 = form.W("chkVideo3d").checked;
  if (d3 && form.W("optVideoLow").checked) form.W("optVideoLow").checked = false, form.W("optVideoMedium").checked = true;
  if (!d3 && form.W("optVideoHigh").checked) form.W("optVideoHigh").checked = false, form.W("optVideoMedium").checked = true;
  form.SetEnabled("optVideoLow", !d3);
  form.SetVisible("imgDisableLow", d3);
  form.SetEnabled("optVideoHigh", d3);
  form.SetVisible("imgDisableHigh", !d3);
  form.SetVisible("imgVideoLow", form.W("optVideoLow").checked);
  form.SetVisible("imgVideoMedium", form.W("optVideoMedium").checked);
  form.SetVisible("imgVideoHigh", form.W("optVideoHigh").checked);
  form.SetVisible("imgVideo3d", d3);
}

// Control type 1 keyboard, 2 joypad, 3 joystick, 4 wheel; Instellen is
// off for the joypad.
void OptionsScreen::Control(int device) {
  temp_.device = device;
  const char* opt[4] = {"optKeyboard", "optJoypad", "optJoystick", "optWheel"};
  const char* img[4] = {"imgKeyboard", "imgJoypad", "imgJoystick", "imgWheel"};
  for (int i = 0; i < 4; i++) {
    form.W(opt[i]).checked = device == i + 1;
    form.SetVisible(img[i], device == i + 1);
  }
  form.SetEnabled("btnControls", device != 2);
}

void OptionsScreen::OnClick(davi::Widget& w) {
  const std::string& n = w.name;
  if (n == "optKeyboard" || n == "lblKeyboard") Control(1);
  else if (n == "optJoypad" || n == "lblJoypad") Control(2);
  else if (n == "optJoystick" || n == "lblJoystick") Control(3);
  else if (n == "optWheel" || n == "lblWheel") Control(4);
  else if (n == "lblStartInfoOn") form.W("chkStartInfoOn").checked = !form.W("chkStartInfoOn").checked;
  else if (davi::Lower(n) == "lblunlockall") form.W("chkUnlockAll").checked = !form.W("chkUnlockAll").checked;
  else if (davi::Lower(n) == "lbltrailerchoice")
    form.W("chkTrailerChoice").checked = !form.W("chkTrailerChoice").checked;
  else if (n == "chkVideo3d" || n == "lblVideo3d") {
    if (n == "lblVideo3d") form.W("chkVideo3d").checked = !form.W("chkVideo3d").checked;
    Video();
  } else if (n == "optVideoLow" || n == "optVideoMedium" || n == "optVideoHigh" || n == "lblVideoLow" ||
             n == "lblVideoMedium" || n == "lblVideoHigh") {
    std::string o = "opt" + n.substr(3);
    if (form.W(o).enabled) {
      for (const char* k : {"optVideoLow", "optVideoMedium", "optVideoHigh"}) form.W(k).checked = o == k;
    }
    Video();
  } else if (n == "sldrMusic") {
    form.SetVisible("imgMusicOn", w.pos > 0);
    game.app.audio.SetMusicGain(Gain(w.pos));
  } else if (n == "sldrSound" || n == "sldrVoice") {
    form.SetVisible(n == "sldrSound" ? "imgSoundOn" : "imgVoiceOn", w.pos > 0);
    game.app.audio.Play(&sample_, Gain(w.pos));
  } else if (n == "btnControls") {
    if (temp_.device == 1) {
      auto* ks = game.Screen<KeySettingsPopup>();
      ks->Edit(temp_.keys);
      game.app.ShowModal(ks, [this, ks](int r) {
        if (r == 1) std::copy(ks->keys, ks->keys + 7, temp_.keys);
      });
    } else if (temp_.device == 3 || temp_.device == 4) {
      auto* ss = game.Screen<SensitivityPopup>();
      bool wheel = temp_.device == 4;
      ss->Edit(wheel, wheel ? temp_.wheel_sensitivity : temp_.joy_sensitivity);
      game.app.ShowModal(ss, [this, ss, wheel](int r) {
        if (r == 1) (wheel ? temp_.wheel_sensitivity : temp_.joy_sensitivity) = ss->value;
      });
    }
  } else if (n == "btnVideoAdvanced") {
    // TfrmDisplaySettings (0x47D980): in the port the image upgrades of
    // graphics.ini, on a copy that Options OK stores.
    auto* ds = game.Screen<DisplaySettingsPopup>();
    ds->Edit(gfx_);
    game.app.ShowModal(ds, [this, ds](int r) {
      if (r == 1) gfx_ = ds->settings;
    });
  } else if (n == "btnOk") {
    // CopyControlsToSettings (0x47cf7c).
    bool d3 = form.W("chkVideo3d").checked;
    if (form.W("optVideoLow").checked) temp_.resolution = d3 ? 6 : 0;
    else if (form.W("optVideoMedium").checked) temp_.resolution = d3 ? 3 : 1;
    else if (form.W("optVideoHigh").checked) temp_.resolution = 4;
    temp_.show_start_info = form.W("chkStartInfoOn").checked;
    temp_.unlock_all = form.W("chkUnlockAll").checked;
    temp_.trailer_choice = form.W("chkTrailerChoice").checked;
    temp_.music_volume = form.W("sldrMusic").pos;
    temp_.fx_volume = form.W("sldrSound").pos;
    temp_.voices_volume = form.W("sldrVoice").pos;
    if (Player* p = game.Cur()) *p = temp_;
    game.SavePlayers();
    vracer::SaveGfxSettings(gfx_);
    game.app.SetUpgraded(gfx_.menu_upgrades);
    game.StartMusic();
    game.Goto(game.Screen<MainScreen>());
  } else if (n == "btnCancel") {
    game.StartMusic();  // back to the stored music volume
    game.Goto(game.Screen<MainScreen>());
  } else if (n == "btnHelp") {
    game.Voice("jac_hlp", 3);
  }
}

// ------------------------------------------------------------- KeySettings

KeySettingsPopup::KeySettingsPopup(Game& g) : VrScreen(g, "TfrmKeySettings") {
  form.background = g.assets.Bitmap("pop_keys");
  form.SetPicture("imgBackGround", form.background);
}

static const char* kKeyButtons[7] = {"btnGas", "btnBreak", "btnLeft", "btnRight", "btnTurbo", "btnParkingBrake",
                                     "btnHorn"};

void KeySettingsPopup::Edit(const int* k) {
  std::copy(k, k + 7, keys);
  waiting_ = -1;
}

void KeySettingsPopup::Captions() {
  for (int i = 0; i < 7; i++)
    form.SetCaption(kKeyButtons[i], waiting_ == i ? "?" : KeyName(game, keys[i]));
}

void KeySettingsPopup::OnShow() {
  Captions();
  game.app.SetFocus(form.Get("btnOk"));
}

// A key button waits for the next key; a key not in [CONTROLKEYS] is
// ignored, one already in use gives MENUSTRINGS 6.
bool KeySettingsPopup::OnKey(SDL_Keycode k) {
  if (waiting_ < 0) return false;
  int vk = VirtualKey(k);
  bool ok;
  std::string name = KeyName(game, vk, &ok);
  if (k == SDLK_ESCAPE || !ok) {
    waiting_ = -1;
    Captions();
    return true;
  }
  for (int i = 0; i < 7; i++)
    if (i != waiting_ && keys[i] == vk) {
      waiting_ = -1;
      Captions();
      Message(game, Fmt(game.Str(6), name));
      return true;
    }
  keys[waiting_] = vk;
  waiting_ = -1;
  Captions();
  return true;
}

void KeySettingsPopup::OnClick(davi::Widget& w) {
  for (int i = 0; i < 7; i++)
    if (w.name == kKeyButtons[i]) {
      waiting_ = i;
      Captions();
      return;
    }
  if (w.name == "btnDefaults") {
    static const char* def[7] = {"UP", "DOWN", "LEFT", "RIGHT", "EXTRA1", "EXTRA2", "EXTRA3"};
    for (int i = 0; i < 7; i++) keys[i] = game.stat.GetInt("DEFAULTSETTINGS", def[i], keys[i]);
    Captions();
  } else if (w.name == "btnOk") {
    Close(1);
  } else if (w.name == "btnCancel") {
    Close(2);
  }
}

// ------------------------------------------------------- Joystick / wheel

SensitivityPopup::SensitivityPopup(Game& g) : VrScreen(g, "TfrmJoystickSettings") {}

void SensitivityPopup::Edit(bool wheel, int v) {
  form.background = game.assets.Bitmap(wheel ? "pop_whee" : "pop_joys");
  form.SetPicture("imgBackGround", form.background);
  value = v;
  form.W("sldrSensitivity").pos = v;
}

void SensitivityPopup::OnClick(davi::Widget& w) {
  if (w.name == "btnOk") {
    value = form.W("sldrSensitivity").pos;
    Close(1);
  } else if (w.name == "btnCancel") {
    Close(2);
  }
}

}  // namespace vr
