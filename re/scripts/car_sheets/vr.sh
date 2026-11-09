#!/bin/sh
# Vakantie Racer sheet: bonus cars with the port's paint and wheel rows (mods/race/Car_cfg.ini).
cd "$(dirname "$0")/../../.." || exit 1
exec python3 -I re/scripts/car_sheets.py vr --names re/scripts/car_sheets/vr_names.txt \
  --tex LEAGUE_BONUS=mods/race/local/bonus_z3.tga --tex TOURNAMENT_BONUS=mods/race/local/bonus_dl.tga \
  --wheels LEAGUE_BONUS=8.68,3.83,14.08,8.68,3.83,-13.75 --wheels TOURNAMENT_BONUS=9.00,3.82,14.40,9.00,3.82,-12.70 \
  --mark "LEAGUE_BONUS=lak + wielen: port (CD heeft geen blad)" \
  --mark "TOURNAMENT_BONUS=lak: L1racer2 van A2 Racer IV" \
  --mark "COUNTRY_COP=cars03 past niet (zo op de CD)" "$@"
