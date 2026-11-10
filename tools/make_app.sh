#!/bin/sh
# Maakt "Vakantie Racer.app" in de projectmap: dubbelklikken = het menu,
# en vanuit het menu de races.
# Het icoon (build/VakantieRacer.icns) komt uit het originele VRacer.exe
# op je eigen CD en staat daarom niet in git.
set -e
cd "$(dirname "$0")/.."
APP="Vakantie Racer.app"
rm -rf "$APP"
# Het HD-pack van setup.sh (.hd-pack) brengt de mod-plaatjes, de HD-textures
# en het menu in 4K al mee: dan niets opnieuw maken (zonder het lettertype
# of de andere CD's zou het slechter worden). VRACER_REBUILD_ASSETS=1 maakt
# ze toch zelf.
if [ -f .hd-pack ] && [ -z "$VRACER_REBUILD_ASSETS" ]; then
  echo "HD-pack aanwezig ($(cat .hd-pack)): plaatjes niet opnieuw gemaakt"
else
# Lak van de Z3/DeLorean en de menuplaatjes uit je eigen CD's (mods/*/local).
python3 -I tools/make_mod_assets.py || echo "let op: make_mod_assets.py mislukt (Pillow nodig)"
python3 -I tools/make_texture_pack.py || echo "let op: make_texture_pack.py mislukt (race zonder HD-textures)"
# Het menu in 4K: alle menuplaatjes 4x met Real-ESRGAN (mods/menu/local/hd);
# alleen wat veranderd is. VRACER_SKIP_UPSCALE=1 slaat dit over.
if [ -z "$VRACER_SKIP_UPSCALE" ]; then
  python3 -I tools/upscale_menu.py || echo "let op: upscale_menu.py mislukt (menu blijft scherp, maar met de originele plaatjes)"
fi
fi
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cat > "$APP/Contents/MacOS/VakantieRacer" <<'SH'
#!/bin/sh
# Start het menu (vracer-menu) vanuit de projectmap naast deze app; het menu
# start zelf de races (vracer).
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
BIN="$ROOT/out/build/release/vracer-menu"
if [ ! -x "$BIN" ] || [ ! -x "$ROOT/out/build/release/vracer" ]; then
  osascript -e 'display alert "Vakantie Racer" message "Nog niet gebouwd. Draai eerst ./setup.sh in de projectmap (zie README.md)."'
  exit 1
fi
export XDG_DATA_HOME="$ROOT/userdata"
export VRACER_FULLSCREEN="${VRACER_FULLSCREEN:-window}"
exec "$BIN" --game-dir "$ROOT/iso/data" --mods "$ROOT/mods/menu" > "$ROOT/app.log" 2>&1
SH
chmod +x "$APP/Contents/MacOS/VakantieRacer"
[ -f build/VakantieRacer.icns ] && cp build/VakantieRacer.icns "$APP/Contents/Resources/VakantieRacer.icns"
cat > "$APP/Contents/Info.plist" <<'PL'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Vakantie Racer</string>
	<key>CFBundleDisplayName</key><string>Vakantie Racer</string>
	<key>CFBundleIdentifier</key><string>local.vakantieracer.mac</string>
	<key>CFBundleExecutable</key><string>VakantieRacer</string>
	<key>CFBundleIconFile</key><string>VakantieRacer</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>0.1</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
	<key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PL
codesign --force -s - "$APP" >/dev/null 2>&1 || true
touch "$APP"
echo "OK: $APP"
