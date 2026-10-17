# Dock changelog

Dock's own changes, newest first. Melodee's history up to the fork (Melodee 1.4.1) is in
[docs/MELODEE.md](docs/MELODEE.md) and `docs/RELEASE_*.md`.

## Unreleased

### Engines
- **JUNO** (`firmware/src/eng_juno.c`), a Juno-60-style voice: one DCO (polyBLEP saw, pulse with PWM, sub square an
  octave down, noise, a level each) -> the Juno-60's fixed high pass (off, 154, 339, 720 Hz) -> a 4-pole resonant
  low pass (zero-delay-feedback ladder, soft-saturating) -> VCA, the ADSR shared by both (VCA GATE: HPF "0 GT".."3 GT").
  The track's LFO (with its fade-in delay) moves pitch, cutoff and pulse width; key tracking comes from the matrix
  (slot 4 KEY -> CUT, set by each sound); DTUNE is a per-note drift.
- **Chorus I / II / I+II**, after the published Juno-60 measurements (I 0.513 Hz, II 0.863 Hz over 1.66–5.35 ms,
  stereo, the two taps in opposite directions; I+II 9.75 Hz over 3.3–3.7 ms, mono). One 1 KB line per part. A JUNO part
  with chorus I or II plays in full stereo.
- Sounds: INIT JUNO, **SILK LEAD** (mono, now the Lead's power-on sound in place of PROPHET Nice n Smooth),
  **STR MACHINE** (Kraut: PWM strings, chorus I+II), **STRING WASH** and **WIDE PAD** (Goth), SQUARE LEAD, JUNO BASS,
  JUNO BRASS, GATE ORGAN, GLASS LEAD. HOME's knobs: CUT, RES, REL, CHOR; CC 74 / 71 turn CUT / RES.
- JUNO takes stored engine ID 14 (Melodee's OBXF, retired in 0.11): IDs 16–18 are user-preset namespaces in the
  browser and favourites, and user presets only store engines below 16.
- Cost, host only: a JUNO voice is about 0.6–0.7 of a PROPHET voice (`tests/juno_test.c` prints it), so it charges
  2 voice units to the Prophet's 3. The device's own figure needs the JieLi toolchain (Phase 2).
- `tests/juno_test.c` (in `tests/dock_tests.sh`): every sound plays, stays finite and unclipped (also at full
  resonance on a chord); the filter darkens; chorus I and II widen to stereo, OFF and I+II stay mono; Silk Lead on the
  Lead at power-on. Optional WAVs of every sound: `build/host/juno_test build/juno`.

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

- **Pages** (FOCUS): the header shows the member's badge, the page and its voices (`4/6`); badge letters replace
  track numerals (sound bar, mixer, browser); knob cells are cards with the label in grey, the turning knob's cell
  filled with the member's colour.

- Every screen passes Melodee's layout lint in Space Grotesk (0 findings over 261 screens x 9 palettes): knob
  labels sit 2 px higher so Space Grotesk's Q tail clears an on/off chip. `tests/dock_tests.sh` runs the lint.

- **Which engine?** Choosing a sound on HOME (PRESETS, a Program Change from the Circuit, an undo) swaps the knob
  strip for 2 s for a browse strip: the engine in the member's colour, the category, the place in the list
  ("PROPHET BRASS 7 / 337"). Long Prophet names show whole on HOME ("Formant Seraphim", not "Formant Seraphi").

- HOME's header no longer gives way to an "ENGINE ..." message when a sound changes engine: the browse strip says it.

- HOME's browse strip counts within each engine ("JUNO 2 / 10", then the next engine from 1), not through the
  whole list.

### Band
- **Line-ups:** SELECT on HOME steps Yacht → Kraut → Hot → Mod → Goth and loads each one's Bass, Keys, Lead and drum
  kit (first guesses from today's engines, `firmware/src/dock_lineups.c`). HOME's header names the Line-up and a strip
  shows it ("LINE-UP KRAUT 2 / 5") while SELECT turns. Tempo no longer sits on SELECT; it needs a new place.
- **Sounds by part:** Bass, Keys and Lead browse one and the same list (every engine but the drums, 333 sounds), in
  the same order; the drummer (part 4) browses drum kits only. KNOB 4's engine jump keeps to the same rule. Checked in
  `tests/dock_ui_test.c`.
- **Program Change** from the Circuit: on a member's channel it picks entry bank × 128 + program (Bank Select MSB,
  CC 0) of that member's list, the browser's ALL numbering; past the end of the list it does nothing. Noted in the
  MIDI interrupt, loaded by the UI loop (`ui.c dock_programs`). The emulator self-test sends it over MIDI.
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
