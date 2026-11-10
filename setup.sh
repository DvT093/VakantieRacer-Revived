#!/bin/bash
# Vakantie Racer Revived (macOS, Apple Silicon): van je eigen CD (ISO) naar "Vakantie Racer.app".
#
#   ./setup.sh VRACER.iso        de ISO van je eigen CD
#   ./setup.sh /pad/naar/cd      of een map met de inhoud van de CD
#   ./setup.sh                   opnieuw draaien: slaat over wat al klaar is
#   ./setup.sh --plain ...       zonder kleur en emoji
#
# Stappen: spelbestanden, Homebrew, submodules, bouwen, HD-pack, app.
# Het script gooit je ISO nooit weg en zet geen spelbestanden in git.
set -euo pipefail

# HD-pack (HD-textures, menu in 4K, mod-plaatjes): zip bij de GitHub-release.
HD_VERSION="hd-v1"
HD_URL="https://github.com/DvT093/VakantieRacer-Revived/releases/download/hd-v1/vakantieracer-hd-v1.zip"
HD_SHA256="4d826e85a3b5cdeb5342d22b6c9b0fab6967aee8c6746425972fc245ea974ddd"
# Alleen deze CD-versie werkt (Vakantie Racer v1.00, 30-03-2000).
SPEL_SHA256="6c4a3a196fa8ee3d2a2bf7b437adbc9da5be3be0708eec15d0fce2d4eb99c939"
# Wat de build nodig heeft (Homebrew-namen).
BREW_DEPS="cmake ninja pkgconf sdl3 ffmpeg freetype glslang spirv-cross llvm@22 python@3.14"
# Plus Pillow voor python3 (brew install pillow), gecheckt met een import.

# Overschrijven kan (testen, eigen mirror): VRACER_HD_URL (ook file://...),
# VRACER_HD_SHA256, VRACER_JOBS (parallelle compileertaken, standaard 2).
HD_URL="${VRACER_HD_URL:-$HD_URL}"
HD_SHA256="${VRACER_HD_SHA256:-$HD_SHA256}"
JOBS="${VRACER_JOBS:-2}"

PLAIN=0
SRC=""
for a in "$@"; do
  case "$a" in
    --plain) PLAIN=1 ;;
    -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) SRC="$a" ;;
  esac
done
# Kleur alleen in een echte terminal; emoji wel (ook in Claude Code).
COLOR=1
[ -t 1 ] || COLOR=0
[ "$PLAIN" = 1 ] && COLOR=0

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
START=$(date +%s)

if [ "$COLOR" = 0 ]; then
  B=""; G=""; Y=""; R=""; D=""; N=""
else
  B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; D=$'\033[2m'; N=$'\033[0m'
fi
# e <emoji> <tekst>: met --plain zonder emoji.
e() { if [ "$PLAIN" = 1 ]; then printf '%s\n' "$2"; else printf '%s %s\n' "$1" "$2"; fi; }
step() { echo; e "$1" "${B}[$2/6] $3${N} ${D}($4)${N}"; }
ok() { e "  ✅" "${G}$1${N}"; }
skip() { e "  ⏭️ " "${D}$1${N}"; }
info() { e "  👉" "$1"; }
warn() { e "  ⚠️ " "${Y}$1${N}"; }
fail() {
  echo
  e "❌" "${R}${B}$1${N}"
  shift
  for l in "$@"; do e "  🔧" "$l"; done
  exit 1
}
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }

echo
e "🏖️ 🚗🚐" "${B}Vakantie Racer Revived · macOS (Apple Silicon)${N}"
e "  " "${D}Van je eigen CD naar een app met HD-textures en een menu in 4K.${N}"

# ---------------------------------------------------------------------------
step "💿" 1 "Spelbestanden van je CD" "1 min"
have_game() { [ -f iso/data/spel.dat ] && [ "$(sha iso/data/spel.dat)" = "$SPEL_SHA256" ]; }
if have_game; then
  skip "iso/ is er al en klopt"
else
  if [ -z "$SRC" ]; then
    fail "Geen spelbestanden gevonden." \
      "Geef de ISO van je eigen CD mee: ./setup.sh ~/Downloads/VRACER.iso" \
      "Of een map met de inhoud van de CD: ./setup.sh /Volumes/VRACER"
  fi
  [ -e "$SRC" ] || fail "Niet gevonden: $SRC" "Check het pad (sleep het bestand in Terminal om het pad te plakken)."
  MNT=""
  cleanup() { if [ -n "$MNT" ]; then hdiutil detach -quiet "$MNT" 2>/dev/null || hdiutil detach -force -quiet "$MNT" 2>/dev/null || true; rmdir "$MNT" 2>/dev/null || true; fi; }
  trap cleanup EXIT
  if [ -f "$SRC" ]; then
    MNT="$(mktemp -d /tmp/vracer-cd.XXXXXX)"
    info "ISO openen (alleen lezen): $(basename "$SRC")"
    hdiutil attach -readonly -nobrowse -noverify -mountpoint "$MNT" "$SRC" >/dev/null \
      || fail "Deze ISO kan ik niet openen." "Is het echt een .iso van de CD? Probeer hem eerst in Finder te openen."
    CD="$MNT"
  else
    CD="$SRC"
  fi
  # De CD-root heeft data/spel.dat; een map die zelf de data-map is mag ook.
  if [ -f "$CD/data/spel.dat" ]; then
    FROM="$CD"; TO="iso"
  elif [ -f "$CD/spel.dat" ]; then
    FROM="$CD"; TO="iso/data"
  else
    fail "Op deze CD staat geen data/spel.dat." \
      "Dit lijkt niet de CD van Vakantie Racer (Davilex, 2000)."
  fi
  info "Kopiëren naar iso/ (ca. 600 MB)..."
  mkdir -p "$TO"
  # Zonder de ISO zelf als iemand iso/ als bron gebruikt.
  rsync -a --exclude '*.iso' --exclude '.DS_Store' "$FROM/" "$TO/"
  chmod -R u+rwX iso
  cleanup; MNT=""; trap - EXIT
  [ -f iso/data/spel.dat ] || fail "Kopiëren mislukt: iso/data/spel.dat ontbreekt."
  if [ "$(sha iso/data/spel.dat)" != "$SPEL_SHA256" ]; then
    fail "Dit is een andere versie van de CD: spel.dat klopt niet." \
      "Alleen Vakantie Racer v1.00 (30-03-2000) werkt; de port kent de adressen in die exe." \
      "Gooi iso/ weg (rm -rf iso) en probeer het met de goede CD."
  fi
  ok "Spelbestanden staan in iso/ (spel.dat klopt)"
fi

# ---------------------------------------------------------------------------
step "🍺" 2 "Gereedschap (Homebrew)" "een paar seconden"
[ "$(uname -s)" = Darwin ] || fail "Dit werkt alleen op macOS."
[ "$(uname -m)" = arm64 ] || fail "Dit werkt alleen op een Mac met Apple Silicon (M1 of nieuwer)."
if [ ! -d /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk ]; then
  fail "De Command Line Tools van Apple ontbreken." "Plak dit in Terminal: xcode-select --install"
fi
command -v brew >/dev/null 2>&1 || fail "Homebrew ontbreekt." \
  'Installeer het via https://brew.sh (één regel in Terminal), daarna opnieuw ./setup.sh'
MISSING=""
for f in $BREW_DEPS; do
  brew list --versions "$f" >/dev/null 2>&1 || MISSING="$MISSING $f"
done
if [ -n "$MISSING" ]; then
  fail "Er mist gereedschap:$MISSING" "Plak dit in Terminal: brew install$MISSING" "Daarna opnieuw: ./setup.sh"
fi
PY="$(brew --prefix)/bin/python3"
[ -x "$PY" ] || PY="$(command -v python3)"
"$PY" -c "import PIL" 2>/dev/null || fail "Er mist gereedschap: Pillow (plaatjes in Python)." "Plak dit in Terminal: brew install pillow" "Daarna opnieuw: ./setup.sh"
# make_app.sh en de tools roepen 'python3' aan: die van Homebrew vooraan.
export PATH="$(brew --prefix)/bin:$PATH"
ok "Alles aanwezig"

# ---------------------------------------------------------------------------
step "📦" 3 "SwARc en Davilex-RED (submodules)" "1 min"
if [ -f external/SwARc/CMakeLists.txt ] && [ -f external/Davilex-RED/menu/davi_menu.cmake ]; then
  skip "external/ is er al"
elif [ -d .git ] || [ -f .git ]; then
  git submodule update --init --recursive || fail "Submodules ophalen mislukt." \
    "Check je internet en probeer: git submodule update --init --recursive"
  ok "external/SwARc en external/Davilex-RED opgehaald"
else
  fail "Dit is geen git-map, dus de submodules ontbreken." \
    "Haal het project op met: git clone --recursive https://github.com/DvT093/VakantieRacer-Revived.git"
fi

# ---------------------------------------------------------------------------
step "🔧" 4 "Spelcode omzetten en bouwen" "15-30 min, de eerste keer"
BUILD=out/build/release
LIFT="$BUILD/swarc/tools/lifter_x86/recomp_lift"
# CMakeLists.txt kiest zelf external/SwARc en external/Davilex-RED.
CFG=(--preset release)
if [ ! -f "$BUILD/CMakeCache.txt" ]; then
  info "Configureren (haalt ook een paar kleine bibliotheken op)..."
  cmake "${CFG[@]}" > out_configure.log 2>&1 || fail "Configureren mislukt." "Zie out_configure.log (onderaan staat de fout)."
fi
if [ ! -x "$LIFT" ]; then
  info "De lifter bouwen..."
  cmake --build "$BUILD" --target recomp_lift -j "$JOBS" > out_lifter_build.log 2>&1 \
    || fail "De lifter bouwen mislukt." "Zie out_lifter_build.log."
fi
# Liften: x86-code uit spel.dat naar C++ in generated/ (opnieuw als de
# symbolen, het manifest of de lifter nieuwer zijn).
if [ ! -f generated/.lifted ] || [ symbols/functions.tsv -nt generated/.lifted ] \
   || [ symbols/jumptables.tsv -nt generated/.lifted ] || [ lifter.manifest -nt generated/.lifted ] \
   || [ "$LIFT" -nt generated/.lifted ]; then
  info "spel.dat omzetten naar C++ (liften)..."
  ./lift.sh > /dev/null 2>&1 || fail "Liften mislukt." "Zie out_lift.log."
  touch generated/.lifted
  cmake "${CFG[@]}" > out_configure.log 2>&1 || fail "Configureren mislukt." "Zie out_configure.log."
else
  skip "Spelcode is al omgezet"
fi
info "Compileren (vracer en vracer-menu, $JOBS tegelijk)..."
cmake --build "$BUILD" -j "$JOBS" > out_build.log 2>&1 || {
  grep -m5 -B2 -A6 "error:" out_build.log || tail -20 out_build.log
  fail "Bouwen mislukt." "Zie out_build.log."
}
[ -x "$BUILD/vracer" ] && [ -x "$BUILD/vracer-menu" ] || fail "Bouwen gaf geen vracer of vracer-menu." "Zie out_build.log."
ok "vracer (de race) en vracer-menu (het menu) zijn gebouwd"

# ---------------------------------------------------------------------------
step "🎨" 5 "HD-pack: HD-textures en menu in 4K" "2-5 min downloaden (ca. 590 MB)"
if [ -f .hd-pack ] && [ "$(cat .hd-pack)" = "$HD_VERSION" ]; then
  skip "HD-pack $HD_VERSION staat er al"
else
  ZIP=build/release/vakantieracer-hd-v1.zip
  mkdir -p build/release
  GOT=0
  if [ -f "$ZIP" ] && [ "$(sha "$ZIP")" = "$HD_SHA256" ]; then
    GOT=1
  else
    info "Downloaden: $HD_URL"
    rm -f "$ZIP.part"
    if curl -fL -# -o "$ZIP.part" "$HD_URL"; then
      if [ "$(sha "$ZIP.part")" = "$HD_SHA256" ]; then
        mv "$ZIP.part" "$ZIP"; GOT=1
      else
        warn "De download klopt niet (sha256 anders)."
      fi
    fi
    rm -f "$ZIP.part"
  fi
  if [ "$GOT" = 1 ]; then
    info "Uitpakken..."
    unzip -oq "$ZIP" -d "$ROOT" || fail "Uitpakken mislukt." "Gooi $ZIP weg en draai ./setup.sh opnieuw."
    echo "$HD_VERSION" > .hd-pack
    ok "HD-textures (build/texture_pack), menu in 4K (mods/menu/local/hd) en mod-plaatjes staan klaar"
  else
    warn "Het HD-pack kon ik niet ophalen. Geen probleem: ik maak de plaatjes zelf."
    warn "Dat gaat met Real-ESRGAN op je eigen Mac en duurt langer (30-60 min)."
    warn "De tekst in de menuplaatjes wordt alleen mooi met Gill Sans Ultra Bold (Microsoft Office)."
  fi
fi

# ---------------------------------------------------------------------------
step "🏁" 6 "Vakantie Racer.app maken" "$([ -f .hd-pack ] && echo "een paar seconden" || echo "30-60 min: plaatjes maken")"
tools/make_app.sh > out_make_app.log 2>&1 || fail "De app maken mislukt." "Zie out_make_app.log."
[ -x "Vakantie Racer.app/Contents/MacOS/VakantieRacer" ] || fail "De app is er niet." "Zie out_make_app.log."
ok "\"Vakantie Racer.app\" staat in $ROOT"

MIN=$(( ($(date +%s) - START + 59) / 60 ))
echo
e "🎉" "${G}${B}Klaar in $MIN min! Dubbelklik op \"Vakantie Racer.app\" (of: open \"Vakantie Racer.app\").${N}"
e "  🎮" "In de race: pijltjes sturen, Cmd = turbo, F9 = origineel/HD, Esc = pauze."
e "  🚐" "Caravan kiezen en Alles vrij zet je aan in Opties."
if [ -n "$SRC" ] && [ -f "$SRC" ]; then
  e "  💾" "$(basename "$SRC") heb je niet meer nodig, bewaar hem als back-up of gooi hem weg."
else
  e "  💾" "VRACER.iso heb je niet meer nodig, bewaar hem als back-up of gooi hem weg."
fi
echo
