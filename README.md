# Vakantie Racer voor de Mac

*English below.*

Vakantie Racer (Davilex, 2000) native op een Mac met Apple Silicon. Geen Wine, geen emulator: de spelcode van je eigen CD wordt omgezet naar een echte Mac-app, met HD-textures, een menu in 4K, 60 fps en een paar mods (bonusauto's, Sjaak met de caravan, alles vrij).

In deze repo zitten **geen spelbestanden**. Je hebt je eigen CD of ISO van Vakantie Racer nodig (v1.00 van 30-03-2000).

## Wat je nodig hebt
- Een Mac met Apple Silicon (M1 of nieuwer)
- De ISO van de CD (of een map met de inhoud van de CD)
- [Homebrew](https://brew.sh); setup.sh zegt precies wat je nog moet installeren

## Installeren

```sh
git clone --recursive https://github.com/DvT093/vakantieracer.git
cd vakantieracer && ./setup.sh ~/Downloads/VRACER.iso
```

Of zeg in [Claude Code](https://claude.com/claude-code) in deze map: "hier is de ISO, zet het op".

setup.sh haalt de spelbestanden van je ISO, zet de spelcode om, bouwt de app en haalt het HD-pack op (ca. 590 MB). De eerste keer duurt het 15-30 minuten. Daarna dubbelklik je op **Vakantie Racer.app**. De ISO heb je dan niet meer nodig.

## Toetsen in de race

| Toets | Wat het doet |
|---|---|
| Pijltjes | Sturen, gas, remmen |
| Cmd | Turbo (Ctrl houdt macOS zelf voor Spaces) |
| Alt (Option) | Handrem |
| Spatie | Toeter |
| T | Je karakter zegt iets |
| Esc of P | Pauzemenu |
| F9 | Origineel of HD (in de race en in het menu) |
| F10 / F11 | Alleen anti-aliasing / alleen filtering |
| 1 / 2 / U / 4 | Bumpercamera / achtervolgcamera / vlak bij de auto / andere camera |
| 3 (vasthouden) | Achteruit kijken |
| 5 t/m 0 | Camera achter een andere auto |

## Opties en mods
- **Alles vrij** (Opties): de bonusauto's (Ferrari, BMW Z3, DeLorean) en alle etappes open.
- **Caravan kiezen** (Opties, standaard uit): kies op Quick Race en in de Garage welke aanhanger je trekt. Uit = elke auto rijdt met zijn eigen aanhanger, zoals in het origineel.
- **Opties > Gevorderd**: resolutie, anti-aliasing, filtering en het menu in 4K.
- Zelf modden: zie `mods/` en `CLAUDE.md`.

## Credits en licentie
- Gebouwd op [SwARc](https://github.com/JTM-rootstorm/x86recomp) van JT (MIT), een static recompiler voor x86-games.
- De code in deze repo is MIT (zie `LICENSE`).
- Vakantie Racer is van Davilex en blijft van de rechthebbende. Deze repo bevat geen spelbestanden; het HD-pack is gemaakt uit de originele textures en is alleen bedoeld voor wie de game zelf heeft. Dit project heeft niets met Davilex te maken.

---

# Vakantie Racer for the Mac

Vakantie Racer (Davilex, 2000, a Dutch holiday racing game) running natively on Apple Silicon Macs. No Wine, no emulator: the game code from your own CD is recompiled into a real Mac app, with HD textures, a 4K menu, 60 fps and a few mods (bonus cars, trailers, unlock all).

This repo contains **no game files**. You need your own CD or ISO of Vakantie Racer (v1.00, 30 March 2000).

## What you need
- A Mac with Apple Silicon (M1 or newer)
- The ISO of the CD (or a folder with the CD's contents)
- [Homebrew](https://brew.sh); setup.sh tells you exactly what to install

## Install

```sh
git clone --recursive https://github.com/DvT093/vakantieracer.git
cd vakantieracer && ./setup.sh ~/Downloads/VRACER.iso
```

Or tell [Claude Code](https://claude.com/claude-code) in this folder: "here is the ISO, set it up".

The first run takes 15-30 minutes. Then double-click **Vakantie Racer.app**. You no longer need the ISO.

## Keys in the race
Arrows steer and drive, **Cmd** = turbo, **Alt** = handbrake, **Space** = horn, **T** = your character talks, **Esc/P** = pause, **F9** = original or HD, **F10/F11** = anti-aliasing / filtering only, **1/2/U/4** = cameras, hold **3** to look back, **5-0** = camera behind another car.

## Options
**Alles vrij** (unlock all) and **Caravan kiezen** (choose your trailer, off by default) in Options; resolution, anti-aliasing and the 4K menu under Options > Gevorderd.

## Credits and license
Built on [SwARc](https://github.com/JTM-rootstorm/x86recomp) by JT (MIT). The code in this repo is MIT (see `LICENSE`). Vakantie Racer belongs to Davilex and its rights holder; this repo contains no game files, and the HD pack is derived from the original textures and meant only for people who own the game. This project is not affiliated with Davilex.
