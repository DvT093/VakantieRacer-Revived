# Vakantie Racer - native Mac-port (decomp)

Doel: Vakantie Racer (Davilex, v1.00, 30-03-2000) **native** op Apple Silicon laten draaien, volledig moddable. Geen Wine, geen emulator, doel 60 fps. Methode: static recompilation met SwARc (x86 -> C++ -> clang arm64), daarna functie voor functie herschrijven naar leesbare C zoals REDRIVER2 / OpenRCT2.

## Doel en takenlijst
- Volgorde (zijn keuze): eerst speelbaar via reverse engineering, pas daarna modden.
- Methode (zijn keuze 8 nov 2026, "ik wil 60 fps" + "niet het wiel uitvinden"): static recompilation op **SwARc** (https://github.com/JTM-rootstorm/x86recomp, MIT). Lifter (`recomp_lift`, Zydis) maakt C++ van elke functie in spel.dat, exacte x87, Win32-HLE op SDL3, DirectDraw 7/D3D7 op SDL_GPU (Metal), DirectSound 8 met 3D, DirectInput 8. Bewezen op Dungeon Siege en Call of Duty 1, ook op macOS Apple Silicon. Volg het gameproject-patroon van DS1-Rebuilt (https://github.com/JTM-rootstorm/DS1-Rebuilt): `recomp_add_lifted` + `GameConfig` + `recomp::RunProcess`. SwARc vastzetten op een commit.
  - Eigen werk: DirectX 5-adapters (IDirectDraw2, IDirectDrawSurface2, IDirect3D2, IDirect3DDevice2/Texture2/Viewport2/Material2, IDirectInputDevice2A) op SwARc's DX7/DInput8-implementatie. Bij voorkeur upstream aanbieden.
  - Geen Wine, geen emulator, geen eigen vertaler, geen binaire patch op het origineel.
- 60 fps: check fps-cap en natuurkunde per frame in `Gm_main.cpp`/`car_phys.cpp`; de game heeft een eigen fps-teller.
- Mod-doelen die elke keuze sturen:
  1. Sjaak (Volvo, auto 3) trekt de tokkie-caravan: `init_player_car` @ 0x439fa0 en `FUN_0041cfc0` vervangen door C die een aanhanger-tabel leest.
  2. Autoselector met alles vrij: native menu schrijft `[PLAYER] car=N` in `game.ini`.
  3. Ferrari (5), BMW Z3 (6) en DeLorean (7) activeren: het originele menu biedt ze nooit aan (alleen 5 `CAR_*.bmp`). Per auto checken: rijdt hij, natuurkunde (`Phys*.dat`/`cars*.dat`/`Car_cfg.ini`), geluid, schademodel, HUD-portret; ontbrekende delen aanvullen via tabellen. Menuplaatje renderen uit het model op de CD.

## Europe Racer komt hierna (belangrijk)
Diederick wil na Vakantie Racer ook Europe Racer (Davilex, 2001, vermoedelijk dezelfde RED-engine) porten. Houd drie lagen gescheiden:
1. **Algemeen Windows/DirectX** (DirectDraw2/Direct3D2/DirectInput 5-adapters, `mixer*`, `GetPrivateProfile*`, `LocalAlloc`, ...): in SwARc (worktree `../SwARc-dx5`), zonder gamenamen, zoals SwARc's CLAUDE.md eist.
2. **RED-engine** (archief `rcs.IMG/.ind`, `.d3d`-modellen, ini-structuur, autotabellen, aanhangers): generiek schrijven en benoemen (geen "vakantie" in namen), later naar een eigen repo `../Davilex-RED` die beide games gebruiken. Scripts in `re/scripts/` zijn al RED-generiek.
3. **Vakantie Racer-specifiek** (adressen in spel.dat, GameConfig, menu-BMP's, mods voor Sjaak e.d.): alleen hier.
Bij elke nieuwe functie: vraag eerst in welke laag hij hoort.

## Bouwen en draaien
- Eenmalig: `cmake --preset release -DSWARC_DIR=$HOME/Projects/Games/SwARc-dx5` (presets zetten de macOS SDK-headers goed: Command Line Tools hebben een halve libc++-map).
- Liften (na wijziging lifter of symbolen): `./lift.sh`. Het zet eerst de handmatige jumptables uit `re/jumptables_manual.tsv` in `symbols/jumptables.tsv` (de Ghidra-export laat die doelen leeg; `symbols/` staat in .gitignore, dus de fix leeft in die getrackte tsv) en draait dan `recomp_lift` (log in `out_lift.log`). Een nieuwe ontbrekende tabel (`FATAL: indirect branch to unlifted address`) zet je in die tsv, niet in `symbols/`. Symbolen: `symbols/` uit Ghidra (`ExportFunctions.java`/`ExportJumpTables.java` uit SwARc `tools/ghidra`, op `re/ghidra/VRacer`) + `pe_symbols.py` met `/opt/homebrew/opt/llvm@22/bin` op PATH.
- Iteratie: `./run.sh` (incrementele build + start, log in `run.log`, `VR_TIMEOUT` seconden). Een ontbrekende Windows-functie stopt het spel met `FATAL: unimplemented import dll!Naam`: implementeer hem generiek in de SwARc-worktree `../SwARc-dx5` (branch `legacy-dx5`).
- Stand 8 nov 2026: de race rendert (Direct3D 2) en is te besturen met de pijltjestoetsen (game.ini `[INPUT]`: VK-codes, gas = Up). Het spel heeft geen eigen framelimiter en koppelt de simulatie aan het frame: `main.cpp` zet `max_fps = 60`; `VRACER_MAX_FPS=0` haalt de limiet weg. Scripted invoer: `VRACER_TEST_INPUT=bestand` (SwARc `user32_window.cpp`, o.a. `<t> key Up 10`), screenshots: `VRACER_SHOT_EVERY=n VRACER_SHOT_DIR=...`.
- De 12 "unimplemented sites" in de lift zijn tekst die als code werd aangezien (adressen zijn ASCII), niet uitgevoerd.

## Speldata
- `run.sh` zet `XDG_DATA_HOME=userdata/`: wat de game schrijft (eigen kopie van `game.ini`, saves, crashrapporten) staat in `userdata/vracer/` in dit project, niet in Application Support (zijn wens). Zodra de game een bestand heeft geschreven, leest hij voortaan die kopie: test `car=5/6/7` dus in `userdata/vracer/install/game.ini`. Alleen-lezen bestanden (`Car_cfg.ini`) pas je in `iso/data` aan.

## Wat er is
- `iso/` - uitgepakte originele CD (gitignored, nooit committen). Bron: `~/Downloads/VRACER.iso` (volume label `VRACER`).
  - `iso/data/VRacer.exe` (36 KB) - launcher, start `MenuData/Menu.exe`.
  - `iso/data/MenuData/Menu.exe` (6,4 MB) - menu, **Delphi** (DirectDraw + DirectSound, BMP-schermen, `menu.ini`). Plan: native herschrijven, niet decompileren.
  - `iso/data/spel.dat` (1,1 MB) - **de eigenlijke game**, PE32 MSVC C++, imagebase 0x400000, entry 0x4c9338. Davilex "RED"-engine (zelfde familie als A2 Racer / Autobahn Raser).
  - Assets: `rcs.IMG/.ind/.CHK` (108 MB, hoofdarchief), `Rc.img/.ind`, `etappe0-5.*` (rmp/omp/cmp/dmp/pmp/ibi/pth = levels), veel leesbare `.ini`'s (auto's, AI, HUD, geluid).
- `re/source_files.txt` - 148 originele bronbestandnamen, gelekt via assert-strings (`D:\Work\RED\Game\prog\car_phys.cpp` etc.). Gebruik dit om functies aan bestanden toe te wijzen.
- `re/scripts/pe_imports.py` - PE-importlijst. `re/scripts/Summary.java` - Ghidra headless postscript.
- `re/ghidra/VRacer.gpr` - Ghidra-project met spel.dat geanalyseerd (5857 functies, 2381 auto-benoemd).

## Platformoppervlak van spel.dat (wat we moeten vervangen)
137 imports totaal: USER32 29, KERNEL32 86 (grotendeels MSVC CRT), GDI32 7, WINMM 8 (mixer + timeGetTime), ADVAPI32 1, **DDRAW 3, DINPUT 1, DSOUND 2**. Direct3D (Immediate Mode, DX5/6-tijdperk) komt via `IDirectDraw::QueryInterface`. Geen DirectPlay-import.

## Tools
- **REA** (Reverse Engineer Anything, MCP `rea`, user scope) op **Ghidra 12.1.4** in `~/Tools/ghidra_12.1.4_PUBLIC` (arm64-decompiler zelf gebouwd in `Ghidra/Features/Decompiler/os/mac_arm_64`), JDK 21 (`/opt/homebrew/opt/openjdk@21`), Node 24 (asdf 24.12.0). Check: `rea doctor --provider ghidra --json`.
- Ghidra headless: `export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home PATH=$JAVA_HOME/bin:$PATH MAXMEM=2G; ~/Tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless re/ghidra VRacer -process spel.dat -noanalysis -scriptPath re/scripts -postScript X.java`

## Regels
- **Laptop 16 GB**: max één zware job (Ghidra `MAXMEM=2G` of één build), nooit parallel. Pauzeren bij <2 GB vrij.
- Originele data en exe's nooit committen of publiceren. De port leest de data van de gebruiker z'n eigen CD/ISO.
- Elke gedecompileerde functie: originele adres in een comment (`// 0x4xxxxx`), in het bestand uit `re/source_files.txt` waar hij thuishoort.
- Verifieer gedrag tegen het origineel (screenshots/waarden), niet op gevoel.
