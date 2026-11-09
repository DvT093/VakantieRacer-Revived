#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
exec python3 -I re/scripts/car_sheets.py a2 --names re/scripts/car_sheets/a2_names.txt \
  --skip TRAILER01 --skip TRAILER02 --skip TRAILER03 \
  --mark "TOURNAMENT_BONUS=gaten = sleutelkleur in het blad" "$@"
