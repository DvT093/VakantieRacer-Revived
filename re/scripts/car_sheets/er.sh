#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
exec python3 -I re/scripts/car_sheets.py er --names re/scripts/car_sheets/er_names.txt \
  --skip PATH_POLITIE_ANIM \
  --mark "America_Classic_Anim=A2-model; wiel che_wie op tech.pcx klopt niet" \
  --mark "DUMMY_ANIM=placeholder: JP/RU/DE/FR/IT klassiek" \
  --mark "PATH_POLIZEI_ANIM=A2-restant: police.pcx past niet" \
  --mark "POLICE02_ANIM=A2-restant: police.pcx past niet" "$@"
