#!/bin/sh
# Lift spel.dat naar generated/. Zet eerst de handmatige jumptables
# (re/jumptables_manual.tsv) in symbols/jumptables.tsv: de Ghidra-export
# laat die doelen leeg en overschrijft ze bij elke nieuwe export.
set -e
cd "$(dirname "$0")"
python3 - <<'PY'
import os
manual = {}
for line in open("re/jumptables_manual.tsv"):
    line = line.split("#", 1)[0].strip()
    if line:
        jmp, targets = line.split("\t")
        manual[jmp.upper()] = targets.upper()
path = "symbols/jumptables.tsv"
rows = open(path).read().splitlines()
seen = set()
for i, row in enumerate(rows):
    jmp = row.split("\t")[0].upper()
    if jmp in manual:
        rows[i] = jmp + "\t" + manual[jmp]
        seen.add(jmp)
rows += [j + "\t" + t for j, t in manual.items() if j not in seen]
open(path, "w").write("\n".join(rows) + "\n")
print("jumptables: %d handmatig ingevuld" % len(manual))
PY
out/build/release/swarc/tools/lifter_x86/recomp_lift --exe iso/data/spel.dat \
  --symbols symbols --out generated --manifest lifter.manifest > out_lift.log 2>&1
tail -3 out_lift.log
