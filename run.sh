#!/bin/sh
# Bouwt (incrementeel) en start Vakantie Racer native. Log in run.log.
cd "$(dirname "$0")"
# Speldata (gewijzigde game.ini, saves, crashrapporten) in het project, niet in Application Support.
export XDG_DATA_HOME="$PWD/userdata"
cmake --build --preset release > out_build.log 2>&1 || { grep -m5 -B2 -A6 "error:" out_build.log; exit 1; }
timeout "${VR_TIMEOUT:-40}" out/build/release/vracer --game-dir "$PWD/iso/data" "$@" > run.log 2>&1
echo "exit $?" >> run.log
