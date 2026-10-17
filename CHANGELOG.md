# Dock changelog

Dock's own changes, newest first. Melodee's history up to the fork (Melodee 1.4.1) is in
[docs/MELODEE.md](docs/MELODEE.md) and `docs/RELEASE_*.md`.

## Unreleased

### Design
- **Style guide** `docs/design/DOCK_STYLE.md`: colour = band member, shapes not numbers, calm screens, nothing under
  16 px or cut; tokens, type roles, the 240 × 240 grid, components, and what changed from the scamps and why.
- Fonts: Space Grotesk (words), Unbounded (big numbers, the DOCK wordmark), Space Mono (data), in `assets/fonts/`.
- Reference screens HOME, FOCUS, FX, LOOPER at 240 × 240 (`docs/design/dock/ref/`, from `tools/dock_refs.py`,
  which flags any text that doesn't fit).

- **Firmware fonts:** a `dock` preset in `tools/gen_aa_font.py` (now the build's): Space Grotesk in the four
  type roles, plus new roles for Unbounded figures (`TY_NV` 28, `TY_NH` 48), the wordmark (`TY_WM`) and Space Mono
  (`TY_MN`, `TY_MB`). 44.9 KB of glyphs (Melodee's Rubik set: 31.4 KB). Rubik's output is unchanged.
- **DOCK palette**, the default: the style guide's tokens, the band's colours as the four track colours (passes every
  contrast check). DAY moves up to second, beside it.

- **HOME** to the reference (`firmware/src/ui_dock.c`, in Melodee's Stage's place): the DOCK wordmark, the
  Line-up, the clock and the BPM; the knob strip always there; a row per part with its badge (dimmed while silent),
  its sound and a five-step meter of its level; the band's eight voices by owner, lit while sounding. Melodee's
  "MIDI IN -> T3" hint is off (in Dock every channel plays its member).

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
