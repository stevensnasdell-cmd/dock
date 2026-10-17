# Dock changelog

Dock's own changes, newest first. Melodee's history up to the fork (Melodee 1.4.1) is in
[docs/MELODEE.md](docs/MELODEE.md) and `docs/RELEASE_*.md`.

## Unreleased

### Band
- **Voices by member** (`firmware/src/voice.c`, `DOCK_VOICES`): Bass 1, Keys 6, Lead 1 voices of their own (8),
  never taken by another part; a member over its share steals only from itself (oldest released first, the lowest
  note of a chord last). The drummer (part 4) plays in its own lane of 6 hits, outside the band's voices.
  `tests/dock_voice_test.c` checks it (and fails on Melodee's shared budget, `-DDOCK_VOICES=0`).
  Melodee's own host tests assume the shared budget: run them with `-DDOCK_VOICES=0`.

### Emulator
- On-screen FM-1 in the browser (`emu/`): the real firmware (DSP, sequencer, UI) compiled to WebAssembly, with
  clickable knobs, buttons and keys, the 240 × 240 screen, and Web MIDI from the Circuit. Page and design from
  Felucca's browser build.
- Proof that the browser plays the same sound as a native build: `emu/build.sh test` renders a MIDI script both
  ways and compares the bytes.

### Project
- Forked from Melodee 1.4.1. Brief (`DOCK.md`), Dock README and this changelog; Melodee's README kept as
  `docs/MELODEE.md`.
