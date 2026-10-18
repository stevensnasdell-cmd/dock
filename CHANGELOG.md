# Dock changelog

Dock's own changes, newest first. Melodee's history up to the fork (Melodee 1.4.1) is in
[docs/MELODEE.md](docs/MELODEE.md) and `docs/RELEASE_*.md`.

## Unreleased

### The macros: Tone, Bite, Shape, Space (job 3 of 3)
HOME's four knobs are now Dock's macros on every engine (DOCK.md §6; `firmware/src/dock_macro.c`). Under
`DOCK_BAND`: with `-DDOCK_BAND=0` HOME's knobs and the sound are Melodee's.
- **KNOB 1–4 on HOME: Tone, Bite, Shape, Space.** Each is an offset, -64 .. +63, from the sound as made; **the
  middle is the sound exactly as saved**. Nothing is overwritten: each engine reads its values through the macros
  where it uses them, so its own pages keep the sound's values and still change it. A detent is 2 steps (the
  middle to an end in 32).
- What each moves, by engine: **JUNO** cutoff / resonance and PWM / attack and release / the room sends;
  **PROPHET** cutoff / resonance and poly-mod (half) / the filter envelope's amount, decay and release / sends;
  **EP** TONE / BARK / decay (release half) / TREM depth and sends; **organ** the drawbars (+ the bars above 4' up,
  − the bars from 8' up down) / drive / the percussion's decay / the rotor fast past +32, slow past −32 (a sound
  with its rotary on) and sends; **PLUCK** brightness / pluck position / decay / sends; **FM6** the modulators'
  index / their attack peak and level, and the feedback / decay and release / sends; **CZ-1** DCW depth / the DCW
  envelope's sweep (x4 .. /4) / decay and release / sends; **SID** cutoff / resonance and pulse width / decay and
  release / sends; a **kit** its TONE / DRV / DECY / sends. Tone moves a filter about ±4 octaves at the ends; a
  level (resonance, drive, bark, the sends) moves toward its top or toward nothing, as the common ENV's levels.
- How: Tone and Bite ride the voice's filter and shape modulation (voice.c, `vmod_t` cutoff / shape) on the
  engines that read them (JUNO, FM6, CZ-1, PLUCK, SID), and the value itself where the others read it (the
  Prophet's value cache, EP's strike, the organ's block, the kit's lanes). **Shape adds its share to the common ENV
  offsets** (`fr_off`, job 2's wiring), so it reaches every envelope the ENV page does. **Space** is the part's room
  sends as heard (fx.c, mix.c).
- **PART / BAND: tap HOME on HOME** turns PART → BAND → the chord wheel → PART. PART moves the selected member;
  BAND moves Bass, Keys and Lead, each from its own value (Space moves the drummer's too); on the chord wheel the
  knobs stay the band's. HOME's header says **PART** (grey) or **BAND** (white) after the BPM (docs/design/README.md,
  Status indicators); the Line-up's name drops a size when it needs the room ("BAND Kraut • DOCK").
- **The knob strip:** Tone · Bite · Shape · Space in Title case, each with a gauge from its middle to the offset (a
  dot at the middle: as made). PART: one bar; BAND: a line each for Bass, Keys and Lead in their colours. The turning
  cell fills with the member's colour (BAND: white, the whole band). In Melodee Pixel, Shape and Space read Shp, Spc.
- The macros are the part's: **0 when a sound loads** (PRESETS, a Program Change, a user sound: browsing starts
  neutral), back with Undo, **kept with a saved Line-up** (`dock_lineup_t.macro`, the record's reserved field, so
  every saved Line-up loads as it was; the .dock files' `"macros"`, -64 .. 63) and 0 on a factory one.
- **At the middle nothing changed:** 118 renders (every engine's presets, a sample of each, and the five Line-ups'
  parts in their rooms) are the same sample for sample as the previous commit's build; the balance and tuning test
  passes as before.
- `tests/dock_macro_test.c` (in `tests/dock_tests.sh`), 13 sounds over every engine: each macro moves the sound;
  Tone + brighter than Tone − (the energy of the signal's changes, e.g. JUNO STR MACHINE −24 / −18 / −13 dB at
  −40 / 0 / +40, PLUCK UPRIGHT −30 / −19 / −13); Shape + rings longer (JUNO STR MACHINE −15 / 10 / 29 dB; the organ:
  its percussion's decay); turned and back to the middle, the same render sample for sample; BAND moves all three
  and the drummer's Space; HOME's tap cycle; saved with a Line-up (the same render back), factory 0, a sound load 0,
  Undo. `dock_ui_test` turns the knobs in PART and BAND and taps the cycle; `ui_render` gains five HOME scenes (0
  lint findings over 279 screens); the emulator's self-tests turn macros (`emu/build.sh test`: native == wasm; the
  panel self-test turns KNOB 1 and hears it). The page's reload test now saves a macro through a real reload.
- Not done: the ranges want tuning by ear (some Tone moves are subtle: FM6, CZ-1, the organ); EP's Tone and Bite act
  on the next strike (as its own TONE and BARK); JUNO's chorus is a switch (I, II, I+II) and stays on its page, not on
  Space; hold LFO + a HOME knob (momentary) does nothing yet (the hold-LFO step); CC 74 / 71 still turn the engine's
  own cutoff / resonance; Save as user keeps the sound without the macros (as the ENV / LFO offsets).

### Common ENV and LFO (job 2 of 3)
ENV and LFO now mean the same thing on every engine (docs/design/DOCK_SIMPLIFY.md: a front door, never a lowest
common denominator). Under `DOCK_BAND`: with `-DDOCK_BAND=0` Melodee's pages and sound are unchanged.
- **ENV page 1** (ATK DEC SUS REL) and **LFO page 1** (RATE DEPTH) on every engine. Each is an offset, -64 .. +63,
  that nudges the sound's **own** envelopes and LFOs from where the sound has them; **0 is the sound as made**. Times:
  16 steps = twice as long (RATE: twice as fast), so the ends are x16 and /16; levels (SUS, DEPTH): + toward the top,
  - toward nothing. Nothing is overwritten: the engine's own pages keep the sound's values and still change it.
- What moves, by engine (`firmware/src/dock_front.c`): PROPHET both envelopes (filter and amp) and its LFO's rate and
  amount; FM6 every operator's EG (R1, R2 R3, L3, R4) and its LFO's speed and depths; CZ-1 the DCW and DCA envelopes of
  both lines (stage 1, the stages to the sustain point, its level, the stages after) and its vibrato; EP DECAY,
  RELEASE and TREM; organ the ADSR and the percussion's decay; PLUCK the ADSR and the string's DAMP; JUNO and SID the
  ADSR (SID: the chip's envelope); a drum kit its DECY. On every engine the part's own LFO moves with RATE and DEPTH
  too. DEPTH on a sound with no LFO brings in the main target: the engine's own LFO where it is routed, else a vibrato.
- A control that means nothing on an engine reads **"--"**, greyed, and a turn does nothing (EP: ATK, SUS; a kit: all
  but DEC); a page with none is not shown (a kit: no LFO page, no ADSR or ENV DEST, which did nothing there).
- **The pages:** ENV opens on the common page, then the engine's own (Prophet: FLT ENV, AMP ENV, ENV MOD; FM6: EG
  RATE, EG LVL, PITCH EG, PITCH LV; CZ-1: the line's rates and levels; EP: BODY, with DECAY and RELEASE), then the
  part's **ADSR** (Melodee's ENV page, renamed) and ENV DEST where they act. LFO opens on the common page, then the
  engine's own (Prophet: LFO, WHEEL; FM6: FM LFO; CZ-1: VIBRATO), then the part's **LFO WAVE** and **LFO SYNC**
  (Melodee's LFO and LFO 2, renamed), LFO DEST, MOD last. SELECT on the common page turns through the same list.
- The pictures: the envelope or wave as made, thin, and the nudged one over it in the knobs' colours (a struck sound,
  EP or a kit: a hit, no sustain).
- The offsets are the part's: 0 again when a sound loads (PRESETS, a Program Change, a user sound), back with Undo,
  **kept with a saved Line-up** (`dock_lineup_t.front`, at the end of the record; the .dock files' `"env"` and
  `"lfo"` per part, optional) and 0 on a factory one. Your Line-ups saved before this load as they were, offsets 0.
- Measured first (host, the generic ADSR / LFO pushed to their ends, as job 1 found): the old ENV page did nothing at
  all on PROPHET, FM6, CZ-1, EP and the kits (0.0 dB), the old LFO RATE nothing on PROPHET, FM6, CZ-1. Now every
  control moves every engine it means something on (`tests/dock_front_test.c`), and "+" is longer: release at -32 / 0
  / +32, PROPHET 290 / 1190 / 4720 ms, FM6 290 / 1200 / 4410, CZ-1 30 / 100 / 410, JUNO 70 / 270 / 1130, EP 40 / 90 /
  520; decay CZ-1 610 / 2390 / 6000+, the 808 kit 60 / 250 / 990.
- **At 0 nothing changed:** 211 renders (every engine's presets, a sample of each, and the five Line-ups) are the same
  sample for sample as before these offsets existed (checked against the previous commit's build); the balance and
  tuning test passes as before.
- `tests/dock_front_test.c` (in `tests/dock_tests.sh`): every control on every engine moves the sound, greyed ones do
  nothing; "+" longer; nudged and back to 0, the same render; the engine's own page after the common one still changes
  the sound with the offsets set; the pages each engine visits; EP's "--" cells; saving, factory, a sound load, Undo,
  an old record. `ui_render` gains the common pages (nudged, EP, a kit) and the ADSR (0 lint findings).
- Wiring for the macros: every engine reads its offsets through one function (`fr_off`), so the Shape macro (job 3)
  can add its share there and reach every envelope the same way.
- Not done: Save as user keeps the sound without the offsets (they stay the part's until the next sound): baking them
  into a user sound needs each engine's inverse (a follow-up). EP: a tine still ringing that is struck again keeps its
  decay (the engine's own behaviour, as with its DECAY knob): a change shows on the next fresh strike.

### Panel (the simplification)
The approved map, `docs/design/DOCK_SIMPLIFY.md` (job 1 of 3; the common ENV / LFO pages, above, and the macros next).
All under `DOCK_BAND`: with `-DDOCK_BAND=0` the panel is Melodee's.
- **Gone from the panel:** SCL's pages (SCALES, SCL, CHORD, MPC) and its hold layer (SCL does nothing until it becomes
  split / layer); SAVE > PROJECT; EDIT's STORE pages (FM6's and the Prophet's: Save as user does it); the sequencer's
  pages (PATTERN, PATTERNS, NOTES, MOTION, SONG, DRUM HIT, TEMPO); Sound designer; Favourite and the browser's FAVES
  (ALL, RECENT and the categories stay); the menu's AUDIO CLICK, CLICK LEVEL, COUNT-IN, NOTE PREVIEW, CHORD ENTRY,
  AUTOSAVE, HOME SELECT, GLO SELECT (and their effects: no click, count-in or project autosave on Dock); the
  "HOLD [GLO] QUICK" hint. The code stays, for Melodee's own tests.
- **SEQ** opens the **CLOCK** page, tempo's new home: KNOB 1 the BPM, 2 the clock source (internal, USB, TRS), 3 the
  tempo coming in (shown while an outside clock leads, else "--"); the panel shows the tempo large, who sets it and
  whether it runs.
- **FX** lands on **SHARED** (the Line-up's room) first, then its sends, then the member's own rack and slots, round
  again; OCT- on SHARED goes HOME.
- **HOME's menu** (hold HOME) has a **Band** section: Save line-up, Factory line-up (moved from HOME's sheet; hold SAVE
  on HOME still saves), **Voice split** (the band's eight voices as B / K / L: 1/6/1, 2/5/1, 1/5/2, 2/4/2, 3/4/1,
  1/4/3; kept with the Line-up when you save) and **Channels** (the members' MIDI channels: B1 K2 L3 D4, the selected
  member, or B5..D8 and on). The sheets (OCT+ held) now hold just Init sound, Save as user, Undo last load.
- **Relabelled:** the hold-GLO layer's mutes, solos and level cards and SYSTEM's channels say B K L D; GLOBAL's voice
  budget is Dock's voice **SPLIT** (`1/6/1`); the menu's FONT says Space Grotesk.
- **EDIT, one way round:** EDIT opens the section list; KNOB 2 picks a section, OCT+ opens it, SELECT turns its pages
  (only its own), OCT- goes back to the list and from there HOME. **PRESETS** browses the member's sounds on every
  page (no section jumps on EDIT pages). HOME tapped on HOME still turns to the chord wheel.
- Kept, though the map listed it: FM6's EDIT **SCALE** page is its operators' keyboard level scaling (break point,
  depths, rate scaling), a sound setting rather than a tuning, so it stays.
- Tests: `tests/dock_ui_test.c` walks every member, every engine it plays and every page button (each section on
  EDIT) and checks no removed page is reached (91 pages are); the moved items where the map puts them; the Band rows;
  EDIT's way round. `dock_save_test.c` saves and reverts through the Band rows; `ui_render` gains the CLOCK, Band and
  GLOBAL screens (0 lint findings over 269 screens x 9 palettes).

### Engines
- **Organ (WHEEL) and PLUCK (PHYS)** are back: Felucca's tonewheel organ and physical models (Leo Kuroshita; PLUCK's
  string and resonators port DaisySP and Rings, MIT), which Melodee had switched off. `DOCK_ORGAN_PLUCK` (dock.h)
  turns on just these two (`WHEEL_PHYS_ON`, core.h); PHASE, VOICE, TRIO and NOISE stay out. Stored IDs as before:
  WHEEL 7, PHYS 9. Both play on Bass, Keys and Lead (the same list on all three), not on the drummer; PHYS shows as
  **PLUCK**. Melodee's comments had called both "Melodee's own design": they say Felucca's again (AGENTS.md).
- Organ sounds (appended after Felucca's five): **MOD HAMMOND** (888800000, 3rd percussion, drive, slow rotor),
  LESLIE FAST, BALLAD SLOW, **VOX COMBO** (reedy slope, buzz, light vibrato) and FARFISA (thin, bright, deeper
  vibrato). The combo organs' vibrato is the part's LFO, set when the sound loads.
- PLUCK sounds (appended after Felucca's nine): **UPRIGHT** (dark, short, a finger thump; about 30 dB down after half a
  second) and **SURF TWANG** (bright, plucked near the bridge, rings on).
- PLUCK's string played up to a semitone sharp when dark or damped (DaisySP's string kept Rings' loop-filter
  correction, made for a 2-pole filter, on its 1-pole one): `phys_dsp.c` now corrects for its own filter
  (`PX_STRING_TUNED`, on with Dock; within about 5 cents now). Dark strings also get up to +12 dB of level back, so the
  upright sits with the band.
- HOME's knobs as DOCK.md §6: organ REG (drawbars), DRV, PERC, ROTR; PLUCK BRIT, POS, DAMP, STRC (the room's verb
  comes with the effects). CC 74 / 71: organ TOP / DRV, PLUCK BRIT / POS.
- Cost, host only (`tests/organ_pluck_test.c`, the same notes): a WHEEL voice is about 0.2 of a PROPHET voice (one
  voice unit: plain 32-bit work), a PLUCK voice about 0.36 (two units: its 64-bit multiplies cost more on the FM-1's
  32-bit core). JUNO is about 0.7, EP about 0.5. A device figure needs the JieLi toolchain (Phase 2).
- Memory: a PLUCK part takes 12.6 KiB (three voices' strings), the largest engine state; with PLUCK on all three
  members and the biggest drum kit the band needs 44,632 B of the 48 KiB arena. Melodee's `ram_audio_test` and
  `resources_test` now count each part's own engines (the drummer never holds PLUCK). Flash: both engines together add about
  15 KB on the host (`-Os`: 12.7 KB code, 2 KB data; no big tables), small beside EP's 144 KiB; the device figure
  needs Phase 2. If the device build runs short, FM6 is the agreed first cut.
- `tests/organ_pluck_test.c` (in `tests/dock_tests.sh`): both pickable on the band, not the drummer; every sound
  plays unclipped; organ pitch, drive, rotary, single-trigger percussion and FARFISA's vibrato; upright short,
  twang long and bright, both in tune; key-up; a legato slide on the Bass; cost; the band's memory. The emulator's
  self-test (`emu/build.sh test`, native == wasm) now also plays the organs and the plucks. Melodee's
  `engine_retirement_test` no longer counts WHEEL and PHYS as retired under Dock.
- The ENG setting's list names EP and JUNO (it showed "-" for both).
- **EP** (`firmware/src/eng_ep.c`), Dock's electric piano: the physically modelled Rhodes of
  [FRodhes](https://github.com/leonardoMirabal/frodhes) by Leonardo Mirabal (GPL-3.0), ported as a Melodee engine.
  Per voice a hammer force pulse (from a contact simulation) strikes the tine's modes (the tine-bar pair, two
  overtones; exact-pole fixed-point resonators); the tip swings across an electromagnetic pickup (its flux curve is
  the bark, growing with velocity); key-up brings the damper and a soft felt knock. The part then passes the pickup's
  RLC load and the TYPE's amp / speaker.
- EDIT 1 "TINE": TONE (pickup offset), BARK, BELL (overtones), HAMR (hammer hardness). EDIT 2 "BODY": DECAY, RELEASE,
  TYPE (STAGE, SUITCASE, FELT, DYNO and Dock's **WURLY**), TREM. HOME's knobs: TONE, BARK, DECAY, TREM (DOCK.md §6);
  CC 74 / 71 turn TONE / BARK. TREM is the part's LFO: a Suitcase's stereo vibrato on every TYPE but WURLY, whose
  tremolo is mono. WURLY is a Wurlitzer-ish voicing of the same tine model (off-axis pickup, hard hammer, strong
  overtones, a nasal load and amp, a symmetric odd-harmonic clip): not a reed / electrostatic-pickup model.
- Sounds: INIT EP, **SUITCASE** (now Yacht's Keys and the Keys at power-on, in place of FM6 TINE EP), STAGE 77,
  EARLY FELT, BELL DYNO, **WURLY** (now Hot's Keys, in place of PROPHET Vintage Wurly), BARK STAGE, BALLAD CASE.
- Voices live in Melodee's allocator (voices by member, shares and `DOCK_ENGINE_OK` apply: EP plays on Bass, Keys and
  Lead). The engine owns the amplitude (as FM6): velocity is the hammer's, the ADSR does nothing, a voice ends when
  its modes die away. The same key re-strikes its ringing tine; a steal or a legato move (mono Bass / Lead) fades the
  old note over 64 samples first. Bend, TUNE, A4, glide and vibrato retune the modes (up to about ±4 semitones). The
  sustain pedal is Melodee's (no half-pedal). FRodhes' DRIVE and effects are not in the engine (the rooms come with
  the effects, step 8).
- EP takes stored engine ID 8 (Melodee's GRAIN, retired): its favourites use bytes 0–1 of row 8, below the FX key map.
- Tables: `tools/gen_rhodes.py` (FRodhes' generator, with its physics in `tools/rhodes/`) writes
  `build/gen/rhodes_tables.h`; `tools/build.py` and `emu/build.sh` run it (numpy + scipy; ~20 s, skipped while its
  inputs are unchanged). Newer numpy / scipy can move the last bit of a few coefficients against FRodhes' own header.
- Cost, host only (`tests/ep_test.c`): an EP voice is about 0.45 of a PROPHET voice and 0.7 of a JUNO voice, so it
  charges 2 voice units, as JUNO. Its kernel is 64-bit integer multiplies, dearer on the FM-1's 32-bit core than on a
  computer: the device figure needs the JieLi toolchain (Phase 2). State: 3.3 KB of RAM per EP part.
- Flash: the tables are 147,348 B (143.9 KiB) in the native object: hammer pulses 90.8 KiB (88 keys × 8 velocity
  layers × 66 int16), pickup flux and slope 32 KiB, keys 14.4 KiB, rates 5.2 KiB. That is a quarter of the app body
  (560 KiB, `BODY_MAX`); how much Melodee already uses needs a device build (Phase 2). If it does not fit: FM6, CZ-1
  and SID carry only a few KiB of tables each (cutting them frees mostly code); the EP itself could halve its pulses
  (4 layers, or 8-bit), keep 73 keys (E1..E7, about -18 KiB) or a 512-point pickup table (-24 KiB): each changes the
  sound and wants a listen.
- `tests/ep_test.c` (in `tests/dock_tests.sh`): Suitcase on the Keys at power-on; EP selectable on Bass, Keys, Lead,
  not on the drummer; every sound plays six notes at 100 and 127 bounded and unclipped; the bark (H2 + H3 against H1)
  grows with velocity (A3: H2/H1 -29 dB at 30, -7 dB at 127) and with BARK; key-up is 36 dB down 0.2 s later and the
  voice ends; A4 at 440 Hz and a +2 bend at 494 Hz; Suitcase's TREM stereo, WURLY's mono; 24 fast notes on 8 voices
  stay clean; the mono Lead re-strikes a slide. Optional WAVs: `build/host/ep_test build/ep`.
- Credits: README "Built on", LICENSING.md, the device's credits page, `tools/rhodes/README.md`.
- Melodee's checks that engine 8 is retired (`tests/ui_test.c`, `tests/pattern_test.c`) now check it is EP.
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
- HOME's header: the clock and tempo at the left, the Line-up and the wordmark at the right, reading "Yacht DOCK".
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
- **Your Line-ups.** Hold SAVE on HOME keeps the whole band as your version of the Line-up playing: each part's
  sound and every edit (all its parameters, its engine's own patch: Prophet, FM6 with its settings, CZ-1, a drum kit's
  edits; JUNO, EP, organ and PLUCK keep theirs in the parameters), level, pan, sends and inserts, the room and the
  voice split. From then on SELECT loads yours, and HOME's header shows a dot after its name ("Yacht • DOCK").
  HOME's sheet (OCT+ held) has **Save line-up** and **Factory line-up** (back to the factory one, after a question;
  yours is erased; both since moved to the menu's Band section). Playing, a save stops the transport first (a flash erase stalls the audio), as a project's does.
- Kept in the flash of Melodee's **project slot D** (`firmware/src/dock_store.c`: its A/B object, power-safe, up to 8
  Line-ups of ~1.6 KB): no flash sector was free in the writable store. Projects A–C work as before; slot D shows as
  LINE-UPS and refuses to load, save or erase as a project. RAM: none of its own (the projects' staging buffer).
- **The factory Line-ups are full records too**, the same format as yours: `firmware/lineups/*.dock` (readable text,
  made from the Line-ups as they were, every sound the same: each renders sample for sample as before) ->
  `firmware/src/dock_factory.h` by `tools/dock_lineup_import.py`. A Line-up exported from the emulator becomes a
  factory one with that tool. Room left in the format for the macros' offsets and a drum groove. Flash: 8 KB.
- Restored values are not clamped to their parameters' ranges, so every sound plays as kept: JUNO SQUARE LEAD sets
  AMT4 (its key tracking) to 64, one past that parameter's 63 (a Melodee project's load would clamp it).
- `tests/dock_save_test.c` (in `tests/dock_tests.sh`): every factory Line-up loads exactly its record; save and load
  back restore every value and render the same; Factory line-up asks, then restores the factory one; slot D is left
  alone; a reboot plays yours from the flash; an export through the import tool is the same record, byte for byte;
  `dock_factory.h` matches its files.
- **Yacht's Lead** is now the CZ-1 ELEC.PIANO (picked by ear); it is also the power-on Lead.
- The tuning check scans for the strongest frequency within 100 cents of the note played (autocorrelation misread
  rich low notes and short plucks, flagging in-tune sounds). Goth's Faint Memories wobbles and spreads on purpose and
  is marked as such rather than checked.
- **Bass and Lead strike every note.** They were legato, so a note overlapping the last (the Circuit's ties and
  slides) didn't re-attack: Hot's Surf Twang and Upright bent instead of plucking, plucky basses lost their snap, and
  Yacht's Silk Lead glided into every note. Now MONO: every note strikes, and a sound's glide moves only between
  overlapping notes. Tested in `tests/dock_voice_test.c`.
- **Sounds picked by ear:** Kraut's bass Rubber Bumper (a rubbery filter snap), Mod's bass CZ Elec. Bass 1 (a fingered
  electric), Goth's bass Sticky Plucks (high and picked), Goth's lead Faint Memories (glassy). Shortlisted by
  measurement (tuning, filter snap, sustain), chosen by ear; trims keep them level with the band.
- **Yacht's bass** is now PROPHET Feature My Bass: Funk Bass II (Melodee's default) is a detuned, synced program,
  ~65 cents sharp by design, which sounded out of tune and chiptune-like against the Rhodes. The balance test also
  checks every melodic Line-up sound is in tune (within 25 cents; all are within 8).
- **Rooms:** each Line-up now brings its own effects (DOCK.md §5), recalled with its sounds by SELECT on HOME and at
  power-on (Yacht). They use Melodee's own shared effects (`fx.c`: three slots, CHAIN or SPLIT, the version Melodee
  measured on the device), set per Line-up in `firmware/src/dock_lineups.c` with every part's sends (less on the Bass);
  the parts' own inserts are cleared. Yacht: a warm plate (short dense REVERB, top rolled off) and a 300 ms delay on
  the Lead. Hot: a spring tank and a 110 ms slapback. Kraut: dry, only a dark 340 ms tape echo (ECHO through TAPE's
  drifting filters). Mod: a small short room (REVERB at its smallest, 0.6 s), sent hard. Goth: a huge wash (REVERB at
  full size and depth, ~6 s, its lines drifting), a 367 ms delay and a deep slow CHORUS send widening the Bass and Lead.
  First settings, to tune by ear with the Line-ups. Not a true shimmer (no pitch shift: that needs another buffer).
- **SPRING** (new effect, stored type 10, `DOCK_SPRING`): Felucca's spring tank (Leo Kuroshita; ten stretched allpasses
  in a loop, after Välimäki, Parker and Abel: the chirp and drips) ported into Melodee's effects at a quarter rate.
  SIZE sets the spring (60–120 ms loop), DECAY its ring. One 4.1 KiB buffer. Its loop low-pass is set for the quarter
  rate (Felucca's value dulled a bright note in a fraction of a second); it rings about 2 s.
- **Cost and RAM** (`tests/dock_room_test.c`, host time, not the device): the shared bus costs 0.35 (Kraut) to 1.2
  (Goth) of a PROPHET voice, Yacht 0.8, Hot 0.75, Mod 0.55. Buffers, from the audio arena: Yacht 12 KiB, Kraut 8,
  Hot 12.1, Mod 4, Goth 14; with the engines, the most any Line-up uses is Hot's 42,960 B, inside the 48 KiB floor
  (`fx_state`, 6,960 B, is static as before). The device figure needs the JieLi toolchain (Phase 2); if the device runs
  short, FM6 is the agreed first cut.
- `tests/dock_room_test.c` (in `tests/dock_tests.sh`): power-on and SELECT recall each room; every room renders
  finite and unclipped with the whole band, is heard (its tail), adds at most 3 dB, and dies to silence; the SPRING
  rings on; cost and RAM. `tests/dock_level_test.c` now measures the sounds dry (the return off), as the trims were
  made. Melodee's `fx_test` starts from Melodee's power-on (no shared effects) and covers SPRING. The emulator's
  self-test plays its three passes through rooms (plate + delay, Goth's wash, spring + slapback), native == wasm.
- Found while testing: Mod's Bass (CZ ELEC.BASS 2) keeps sounding long after its key is up (about -2 dB a second).
- **Balance:** the Line-ups' sounds were up to 23 dB apart (the Rhodes, the upright and the SID kits far too quiet).
  `firmware/src/dock_trim.c` trims each measured factory sound to its role's loudness (Bass -24, Keys -27, Lead -26,
  Drums -25 dB RMS alone) before its LEVEL, so the MIXER (tap GLO; ALGORITHM picks the member, KNOB 1 its level)
  still balances on top. `tests/dock_level_test.c` measures every Line-up sound.
- **Line-ups:** Mod's Keys are WHEEL MOD HAMMOND and its Lead VOX COMBO (were PROPHET Leslied, JUNO GATE ORGAN);
  Hot's Bass is PLUCK UPRIGHT and its Lead SURF TWANG (were PROPHET Uptight Bass, CZ ELEC.GUITAR).
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
- **Saves survive a reload:** the emulator now has the FM-1's flash (`emu/dock_flash.c`, Felucca's: a 1 MiB NOR image
  behind storage.c), with Melodee's flash code on (`MELODEE_FLASH 1`). The page keeps the sectors that change in the
  browser's storage and gives them back at the next Switch on: your Line-ups, settings, user presets and projects.
  The page now keeps all of Melodee's writable store (Felucca's list missed the Prophet banks and the project
  extensions). `emu/build.sh web` checks it with the built page pieces (`emu/panel/reload_test.mjs`).
- **Export Line-up** (a button under the panel) downloads the band as it plays as a small text file (`yacht.dock`),
  for `tools/dock_lineup_import.py`.
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
