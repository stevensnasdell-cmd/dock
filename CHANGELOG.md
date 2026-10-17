# Dock changelog

Dock's own changes, newest first. Melodee's history up to the fork (Melodee 1.4.1) is in
[docs/MELODEE.md](docs/MELODEE.md) and `docs/RELEASE_*.md`.

## Unreleased

### Band
- **The band** (`firmware/src/dock.h`, `DOCK_BAND`): always four parts on MIDI channels 1–4 (Bass, Keys, Lead,
  Drums); Bass and Lead play mono legato whatever sound is loaded; power-on sounds are Yacht stand-ins until the
  JUNO and EP engines exist: PROPHET Funk Bass II, FM6 TINE EP, PROPHET Nice n Smooth, 808 KIT.
- **No sequencer** (`DOCK_BAND`): SEQ and REC do nothing yet (no patterns, step pages, pattern recording or
  Capture; DOCK.md gives them the clock status and the looper later); no step dots on HOME; no Add / Remove track.
- **Last-note priority** on Bass and Lead, so a sequencer's slides (new note before the old note-off) play
  straight away; a Prophet sound's own low-note priority no longer applies there.
- `tests/dock_ui_test.c`: the band on the real UI.
- `tests/dock_tests.sh`: Dock's quick host checks (voices, UI on Melodee's rules, emulator).
- **Voices by member** (`firmware/src/voice.c`, `DOCK_VOICES`): Bass 1, Keys 6, Lead 1 voices of their own (8),
  never taken by another part; a member over its share steals only from itself (oldest released first, the lowest
  note of a chord last). The drummer (part 4) plays in its own lane of 6 hits, outside the band's voices.
  `tests/dock_voice_test.c` checks it (and fails on Melodee's shared budget, `-DDOCK_VOICES=0`).
  Melodee's own host tests assume the shared budget: run them with `-DDOCK_VOICES=0`.

### Emulator
- Fixed harsh noise when turning MASTER down: the emulator's copy of the pot smoothing was unsigned and wrapped to a
  huge volume (the firmware's own is signed). The browser now also plays at the device's level (-6 dBFS ceiling).
- On-screen FM-1 in the browser (`emu/`): the real firmware (DSP, sequencer, UI) compiled to WebAssembly, with
  clickable knobs, buttons and keys, the 240 × 240 screen, and Web MIDI from the Circuit. Page and design from
  Felucca's browser build.
- Proof that the browser plays the same sound as a native build: `emu/build.sh test` renders a MIDI script both
  ways and compares the bytes.

### Project
- Forked from Melodee 1.4.1. Brief (`DOCK.md`), Dock README and this changelog; Melodee's README kept as
  `docs/MELODEE.md`.
