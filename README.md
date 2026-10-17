# Dock

**Onboard a band.**

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

A MIDI synth backing band from the 70s and 80s, in your pocket: custom firmware for the M-VAVE FM-1.
Three band members (**Bass**, **Keys**, **Lead**) plus a vintage rhythm-box drummer, played from an external
sequencer such as the Novation Circuit Tracks (one MIDI channel per member), from the FM-1's own keys, or by a
looper. Not a groovebox: the sequencing happens elsewhere.

> **Status: early development, browser only.** Dock does not install on an FM-1 yet. Everything is built and
> tested in a browser emulator that runs the real firmware code first. The plan and every design decision are in
> [DOCK.md](DOCK.md).

## The band

| Member | MIDI ch | Plays |
|---|---|---|
| **B: Bass** | 1 (Circuit Synth 1) | mono, legato |
| **K: Keys** | 2 (Circuit Synth 2) | poly |
| **L: Lead** | 3 (Circuit MIDI 1) | mono, legato |
| **Drums** | 4 (Circuit MIDI 2) | its own voice lane, never takes a band voice |

Five line-ups ("Docks"), each with its own three sounds and room: **Yacht**, **Hot**, **Kraut**, **Goth**, **Mod**.

## Try it (emulator)

```sh
emu/build.sh web
python3 -m http.server 8000 --bind 127.0.0.1
```

Open <http://localhost:8000/build/emu/panel/> in Chrome, press **Switch on**, plug in your Circuit. Details and
one-time setup: [emu/README.md](emu/README.md).

## Layout

| Path | What |
|---|---|
| `DOCK.md` | the brief: what Dock is, the plan, the decisions |
| `CHANGELOG.md` | Dock's changes |
| `emu/` | the browser emulator (firmware compiled to WebAssembly, on-screen FM-1) |
| `firmware/` | firmware sources: `src/` app, `hal/` hardware layer, `loader/` update loader |
| `tests/` | host tests (golden renders, CPU budgets, UI) |
| `tools/` | build script, generators, installer |
| `docs/MELODEE.md` | Melodee's README, manual and release notes: the firmware Dock is built on |

## Built on

Dock is a modified version of **[Melodee](https://github.com/keremimo/melodee)** by Kerem Kilic (Ellic Studio),
itself a modified version of **[Felucca](https://github.com/hugelton/Felucca)** by Leo Kuroshita (@kurogedelic),
Hügelton Instruments. The emulator's panel page and its design follow Felucca's browser build, after
charlesvestal's [X0X](https://github.com/charlesvestal/fm1-x0x). The EP engine's Rhodes is the physically modelled
voice of **[FRodhes](https://github.com/leonardoMirabal/frodhes)** by Leonardo Mirabal (GPL-3.0), with its table
generator. The organ (WHEEL) and PLUCK (PHYS) engines are Felucca's, which Melodee had retired; PLUCK's string and
resonators are ports of DaisySP (Electrosmith, Emilie Gillet) and Rings (Emilie Gillet), MIT. Each Line-up's room
plays through Melodee's own shared effects (reverb, echo, tape, chorus); its SPRING is Felucca's spring tank, ported.
Full credits for engines,
fonts and ported DSP are
in [docs/MELODEE.md](docs/MELODEE.md#credits) and [LICENSING.md](LICENSING.md).

Dock is not affiliated with or endorsed by Melodee, Felucca, FRodhes, Hügelton Instruments or M-VAVE. "Rhodes",
"Fender" and "Wurlitzer" are trademarks of their owners; Dock's EP is an independent emulation. Please don't report
Dock problems to them.

## Licence

Free software: [GPL-3.0-only](LICENSE). Bundled fonts and ported DSP keep their own licences
([LICENSES/](LICENSES/)); details in [LICENSING.md](LICENSING.md).

Dock modifications: Copyright (C) 2026 Steven Snasdell\
Melodee code: Copyright (C) 2026 Kerem Kilic (Ellic Studio)\
Retained Felucca portions: Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments\
FRodhes' Rhodes model (the EP engine): Copyright (C) 2026 Leonardo Mirabal
