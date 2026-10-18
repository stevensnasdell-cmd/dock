# Dock simplification map

**Done (job 1), 2026-10-18:** the panel matches this map (CHANGELOG "Panel"), with one exception found on the way:
EDIT's **SCALE** page is FM6's operator keyboard level scaling (break point, depths, rate scaling), a sound setting,
not a tuning, so it stays. The Prophet's STORE page went with FM6's (Save as user does both). PRESETS browses sounds
on every page, not only EDIT's.

**Done (job 2), 2026-10-18:** the common ENV and LFO pages (CHANGELOG "Common ENV and LFO"): page 1 of each on every
engine, relative to the sound (`firmware/src/dock_front.c`), the engine's own pages after it; Melodee's ENV, LFO and
LFO 2 pages are now ADSR, LFO WAVE and LFO SYNC.

**Approved by Steven, 2026-10-18**, with the proposed answers to the four questions (Favourites removed, MOD matrix kept
at the end of LFO, metronome/count-in removed until the looper, chord keys removed), and one rule added: common pages
(ENV, LFO) are a **front door, never a lowest common denominator**. Page 1 is a common control on every engine that
nudges the engine's own envelopes / LFOs relative to the sound (the middle = as made); the engine's own pages follow
it unchanged; a control that makes no sense on an engine is greyed out. The visual walkthrough of every button is the
"Dock Button Tour" page.

Every screen, page, sheet, menu entry and layer the firmware can reach, with what happens to it. The test is Dock's
two jobs: **dialling in the sound**, and **shaping it live** while an external sequencer plays. Anything that serves
neither is Melodee's groovebox heritage and goes (removed from the panel; the code can stay compiled out with
`DOCK_BAND=0` for Melodee's own tests). Nothing changes in the firmware until this is approved.

**Keep** = stays, maybe relabelled · **Move** = stays, somewhere it makes more sense · **Remove** = gone from the panel

## Buttons and their pages

| Button | Page(s) | Verdict | Why |
|---|---|---|---|
| **EDIT** | engine pages: EDIT 1/2, Prophet's, CZ-1's (DCO, DCW, DCA, wave, touch…), FM6's (operators, EG, algorithm…), EP's, organ's, pluck's, drum lanes | **Keep** | dialling in: the deep sound |
| EDIT | VOICE (mode, glide, glide mode, priority) | **Keep** | glide and priority matter for Bass and Lead |
| EDIT | STORE (FM6's store to user) | **Remove** | Save as user does it |
| EDIT | SCALE (per-engine tuning) | **Remove** | the sequencer sends the notes |
| **ENV** | ENV, ENV DEST, and each engine's own envelope pages | **Keep**, with a common ADSR page first (relative) | front door on every engine; nothing lost |
| **LFO** | LFO, LFO 2, LFO DEST, and each engine's own LFO pages | **Keep**, with a common LFO page first | front door on every engine; later hold LFO + knob = momentary tweak |
| LFO | MOD (modulation matrix) | **Keep, last** | deep; useful for dialling in, rarely needed |
| **FX** | SHARED (the Line-up's room) + each member's SEND 1–3 | **Keep, first** | the room is the Line-up's sound; tap FX should land here |
| FX | FX 1–3 (each member's own insert effects) | **Keep, after the room** | drive or chorus on one member |
| FX | (hold) | **New** | the performance FX layer (DOCK.md §7), later |
| **ARP** | ARP, ARP 2 | **Keep** | arpeggiate the Keys from the Circuit's chords |
| **SCL** | SCL (scale, root, quantise, transpose), SCALES (scale browser), CHORD (chord keys), MPC (MPC Sample companion) | **Remove** | the sequencer sends the notes; SCL becomes split/layer for the FM-1's own keys, later |
| SCL | (hold) quick layer: root, scale, chord | **Remove** | as above |
| **GLO** | MIXER (tap) | **Keep** | each member's level and pan |
| GLO | GLOBAL: tuning A4, voice budget, clock source, tune | **Keep tuning and clock**; voice budget **replaced** by Dock's voice split | Dock's voices are per member |
| GLO | SYSTEM: MIDI, drum channel, MIDI route, info | **Keep**, relabelled as member channels | which channel plays which member |
| GLO | (hold) quick layer: black keys mute B/K/L/D, keys solo, KNOB 1–4 levels, tap tempo | **Keep**, relabel T1–T4 → B K L D | this is the brief's "hold GLO = mutes": it already exists |
| **SAVE** | PRESETS (the full sound browser) | **Keep** | browsing with the list and category |
| SAVE | USER (user sound slots) | **Keep** | Save as user lands here |
| SAVE | PROJECT (Melodee projects, template, boot project) | **Remove** | Line-ups replace projects |
| SAVE | (hold) | **Keep** | save Line-up (built) |
| **SEQ** | TEMPO (BPM, swing, record quantise) | **Move**: SEQ becomes the clock page: BPM (tempo's new home), clock source, incoming BPM | tempo needs a home since SELECT changes Line-up |
| SEQ | PATTERN, PATTERNS, NOTES, MOTION, SONG | **Remove** (already unreachable) | no sequencer |
| **REC** | (nothing) | **Later** | the looper |
| **PLAY** | start/stop the internal clock | **Keep** | the arpeggiator and later the looper follow it |
| **HOME** | Stage (Dock's HOME) | **Keep** | |
| HOME | tap on HOME: chord wheel | **Keep**, in the cycle PART → BAND → chord wheel | agreed: it's fun |
| HOME | (hold) the menu | **Keep**, trimmed: see below, and gains a **Band** section | |

## HOME's menu (hold HOME)

| Entry | Verdict | Why |
|---|---|---|
| COLOR (palette), FONT (Space Grotesk / Pixel), LIGHTS, HOLD (button hold time), KNOB ACCEL, SCREEN OFF | **Keep** | the instrument's own preferences |
| USB AUDIO, USB INPUT | **Keep** | record the band into a computer |
| CALIBRATION, ABOUT (credits, licences) | **Keep** | hardware and licences |
| AUDIO CLICK, CLICK LEVEL, COUNT-IN | **Remove for now** | metronome for recording patterns; may return with the looper |
| NOTE PREVIEW, CHORD ENTRY | **Remove** | pattern editing |
| AUTOSAVE | **Remove** | it saved projects; Line-ups save when you hold SAVE |
| HOME SELECT, GLO SELECT | **Remove** | SELECT on HOME is the Line-up |
| PICKER POPUPS | **Keep** | the pop-up list when a knob picks a waveform |
| SOUND DESIGNER | **Remove** | Melodee's guided builder; EDIT and the macros do the job |
| **Band** (new section) | **Add** | Save line-up, Factory line-up, voice split (B/K/L shares), member channels |

## Sheets (hold OCT+)

| Sheet | Rows | Verdict |
|---|---|---|
| HOME's / a page's sound sheet | Init sound, Save as user…, Undo last load | **Keep**: about this member's sound |
| | Favourite | **Remove**: invisible from HOME; the engine strip finds sounds |
| | Sound designer | **Remove** |
| | Save line-up, Factory line-up | **Move** to the menu's Band section (hold SAVE still saves) |
| SHARED's (the room) | Routing | **Keep** |
| Song, pattern, patterns, note, hit sheets | all | **Remove** (unreachable already) |

## Screens of their own

| Screen | Verdict | Why |
|---|---|---|
| The sound browser: lists ALL, categories | **Keep** | |
| Its RECENT list | **Keep** | quick way back to a sound you liked a minute ago |
| Its FAVES list | **Remove** with Favourite | |
| NAME (naming a user sound) | **Keep** | |
| MIXER | **Keep** | |
| Scales picker, PATTERNS, SONG, NEW SONG | **Remove** | |

## Questions for you

1. **Favourites:** remove (proposed), or keep and show a star on HOME's rows?
2. **MOD matrix:** keep at the end of LFO (proposed), or remove for a calmer LFO?
3. **Metronome / count-in:** remove now and bring back with the looper (proposed)?
4. **Chord keys** (one FM-1 key plays a chord): remove (proposed), or keep for playing the Keys by hand?

## What it adds up to

- **Removed from the panel:** the scale/chord/MPC family, projects, the sequencer leftovers, Sound designer,
  Favourites, the metronome and pattern settings in the menu, HOME SELECT / GLO SELECT.
- **Moved:** tempo to SEQ; Line-up save/revert to the menu's Band section; the room to the front of FX.
- **Relabelled:** the GLO layer and the member channels say B, K, L, D.
- **Added:** the Band section (voice split, channels).
