## Bevindingen spel.dat
### Menu <-> game
- Menu en game praten via `data/game.ini`: menu schrijft `[ETAPPE] etappe`, `[LEAGUE] league`, `[PLAYER] car` + upgrades (MOTOR, BANDEN, ABS, TURBO, VERSNELLINGSBAK); game schrijft `finished`, `damage`, `tracktime`, `position`, `[RACERS]` terug.
- `[PLAYER] car` = index in `Car_cfg.ini [leagueN]` (0-9). `RACER_%d`-lookup in `init_player_car`.
### Auto's
- `Car_cfg.ini [leagueN]`: 0-5 gewone racers (Ami, Camper, 2cv, Volvo, Jeep, Ferrari), 6 `LEAGUE_BONUS` (BMW Z3), 7 `TOURNAMENT_BONUS` (DeLorean), 8 politie, 9 `veh_08`, 10-12 aanhangers.
- Modellen per auto in `L{n}_anim.ini`: body / `_wie` (wielen) / `_ram` (schade) / lampen.
- Speciale auto's kiezen = menu schrijft `car=6` of `car=7`. Geen game-code nodig (te verifiëren in de draaiende game).
### Aanhangers (caravan)
- `init_player_car` @ 0x439fa0 (`plr_car.cpp`): aanhanger hangt vast aan de autoindex: car 0 (Ami) -> "Caravan", 2 (2cv) -> "Vouwwagen", 4 (Jeep) -> "Jetski"; daarna `FUN_004204b0(player_obj, ?, name)` koppelt hem.
- Tegenstanders: `FUN_0041cfc0` (opponents toevoegen), zelfde regel per racerindex 0/2/4.
- Caravan vrij kiezen = deze keuze in `init_player_car` vervangen door een nieuwe ini-sleutel (bv. `[PLAYER] trailer=`).
### Autonummer -> model (geverifieerd via render, `build/cars_side.png`)
- `[leagueN]` i -> AnimName `RACER_{i+1}` -> `L{n}_anim.ini`: 0 Ami (hatchback, Sjonnie & Anita, de tokkies; met trailer1 erachter, bevestigd door Diederick 8 nov 2026) · 1 Camper (Gebroeders Mook) · 2 2CV (Peder, de hippie) · 3 Volvo (stationwagen, Sjaak) · 4 Jeep (G-klasse-vorm, Smaksie) · 5 Ferrari · 6 BMW Z3 · 7 DeLorean.
- Aanhangers: trailer1 = "Caravan" = Crappy Wohnwagen (tokkie-caravan, achter de Ami) · trailer2 = code-naam "Vouwwagen", in de game de ronde Hippie Wohnwagen (achter de 2CV) · trailer3 = "Jetski" op aanhanger (achter Smaksie's G-klasse). Er is geen echte vouwwagen in de game.
- Menu kent maar 5 speelbare auto's: `MenuData/CAR_JOH.bmp` (Johnny = Sjonnie & Anita, Ami), `CAR_HIP.bmp` (Peder, 2CV), `CAR_OUD.bmp` (oudjes, vermoedelijk Gebroeders Mook, Camper), `CAR_SJA.bmp` (Sjaak, Volvo), `CAR_SMA.bmp` (Smaksie, G-klasse). Ferrari (5), BMW Z3 (6, `LEAGUE_BONUS`) en DeLorean (7, `TOURNAMENT_BONUS`) hebben geen menuplaatje, maar wel complete modellen (+ `_ram`/`_wie`) en de Z3 een eigen geluid `lotaz3.wav`. Diederick kende Z3 en DeLorean niet uit het spel: vermoedelijk verborgen/ongebruikte inhoud. Of ze rijden is onbevestigd tot de game draait. `garage.ini` (Trabant, Astra, Porsche) en de Z3/Aston/TVR/Rolls-commentaren in `Car_cfg.ini` zijn restanten uit andere Davilex-racers op dezelfde engine.
- Ferrari = "Grande Winnaar"-prijs: menu heeft een eigen pop-up `TfrmFerrari2` (456x206, achtergrond `MenuData/POP_GWIN.bmp` met rode Ferrari + "Grande Winnaar", alleen knop "Verder" via `MenuCaps.ini [TfrmFerrari2]`). Niet op het prijzenwiel (`static.ini [wofprizes]`: Sunnies, rem, banden, motor, turbo). Hoofdprijs volgens `static.ini [TRACK5] WINTEXT` = "21 dagen onbeperkt toegang tot Camping Walhalla" bij winst in de laatste etappe (Monaco/Cote d'Azur, alleen 1e plaats telt). Wanneer de Ferrari-pop-up verschijnt is nog onbekend: het menu opent schermen op klassenaam (geen directe VMT-verwijzing, ook niet voor het prijzenwiel), dus de trigger zit in Delphi-code die nog uitgezocht moet worden (Ghidra-project `re/ghidra/VRacerMenu`). Diederick heeft de Ferrari nooit gezien, ondanks lang spelen.
- Let op: de commentaren in `Car_cfg.ini [wheel_positions]` noemen 0 = Camper, 1 = Ami; die zijn verwisseld t.o.v. de modellen.
### Archief
- `rcs.ind`: u16 aantal (2254) + records van 24 bytes (naam[20], u32 offset in `rcs.IMG`). Script: `re/scripts/rcs_list.py`.
- `.d3d`-model (eerste lezing): i32 -1, u32 punten, u32 vlakken, dan per punt 48 bytes (pos xyz, normaal xyz, uv, 4 floats).
