# Dock

**Onboard a band.**

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

![Dock's screens on the FM-1: HOME, a member, FX, the looper (references at 240 x 240)](docs/design/dock/ref/sheet.png)

A MIDI backing band from the 70s and 80s, in your pocket: custom firmware for the M-VAVE FM-1. Four members, **Bass,
Keys, Lead and a drummer**, each on its own MIDI channel, played by an external sequencer such as the Novation Circuit
Tracks. Dock is not a groovebox: it has no sequencer of its own. It does two jobs: **dialling in the right sound** for
each member, and **shaping the band live** while the sequencer plays.

> **Status: Phase 1, browser only.** Dock does not install on an FM-1 yet. Everything is built and tested in a browser
> emulator that runs the real firmware code. Whether it fits the FM-1's flash and CPU is measured in Phase 2. The plan
> and every decision are in [DOCK.md](DOCK.md); changes in [CHANGELOG.md](CHANGELOG.md).

## What it does

- **A band, not tracks.** Bass (ch 1), Keys (ch 2), Lead (ch 3), Drums (ch 4). Each member owns its voices (Bass 1,
  Keys 6, Lead 1); nothing steals from anyone, and the drummer plays in its own lane. Bass and Lead strike every note,
  with last-note priority and glide only on ties, the way a player does.
- **Five Line-ups**, one turn of SELECT apart: **Yacht**, **French**, **Hot**, **Mod**, **Goth**. Each brings its four
  sounds and its own room (reverb, echo, spring, phaser), balanced and in tune out of the box.
- **Save your version** of any Line-up (hold SAVE), go back to factory, and export it as a `.dock` file that can become
  the factory one.
- **Four knobs that always mean the same thing:** Tone, Bite, Shape and Space, on every sound. Tap HOME to shape one
  member (PART) or the whole band at once (BAND).
- **ENV and LFO work on every engine**: a common first page nudges each engine's own envelopes and LFOs; the engines'
  deeper pages are all still there.
- **Sounds:** PROPHET, a Juno-60-style **JUNO** with chorus I / II, a physically modelled **Rhodes and Wurly** (EP), a
  Hammond-style **organ** with rotary, **plucks** (upright bass, surf twang), plus FM6, CZ-1, SID and drum kits. Bass,
  Keys and Lead browse one shared list; the drummer only drum kits. The browse strip names the engine as you turn.
- **Mixer and mutes:** tap GLO for each member's level and pan; hold GLO to mute or solo members on the black keys and
  tap the tempo.

## Try it (emulator)

```sh
emu/build.sh web
python3 -m http.server 8000 --bind 127.0.0.1
```

Open <http://localhost:8000/build/emu/panel/> in Chrome, press **Switch on**, plug in your Circuit. Saves stay in the
browser between reloads. Details and one-time setup: [emu/README.md](emu/README.md). Tests: `tests/dock_tests.sh`.

## What's changed from Melodee

Dock started as a fork of Melodee 1.4.1 on 17 Oct 2026. In order:

1. **The browser emulator**: the real firmware in WebAssembly on Felucca's on-screen FM-1 (identical to a native build).
2. **The band**: voices by member, the drummer's own lane, mono strike-every-note Bass and Lead, the sequencer hidden.
3. **Dock's look**: a style guide, Space Grotesk / Unbounded / Space Mono, the DOCK palette, a new HOME.
4. **New and returned engines**: JUNO, the EP (from FRodhes), the organ and plucks brought back.
5. **Line-ups with rooms**, loudness trims, balance and tuning tests, then **saving and export**.
6. **The panel simplified** (scales, projects, song mode, the sound designer and more removed; tempo on SEQ; a Band
   section in HOME's menu), **common ENV / LFO**, and **the macros** with PART / BAND.

Still to come: the drummer's grooves, the FX performance layer, the looper, A/B compare, a device CPU estimate, then
the hardware build (Phase 2).

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
