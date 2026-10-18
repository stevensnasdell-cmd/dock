# DOCK

**Onboard a band.**

A MIDI synth backing band from the 70s and 80s, in your pocket.

Custom firmware for the M-VAVE FM-1. Three band members (Bass, Keys, Lead) plus a vintage rhythm box drummer, in one pocket box. Played by an external sequencer (Novation Circuit Tracks, a DAW, anything with MIDI), by the FM-1's own keys, or by a looper. Not a groovebox: no step sequencer. The drummer is a preset rhythm box you pick and play, and it steps aside when something else is drumming.

This file is the brief. Build from it; update it when a decision changes.

---

## 1. Principles

1. **Feel and sound first.** Nail the sounds and the hands-on feel in a browser emulator before any hardware work.
2. **One codebase.** The emulator runs the real firmware C code compiled to WebAssembly, so what sounds right in the browser sounds the same on the device.
3. **Curated, not everything.** A band with a clear identity beats thirteen engines.
4. **Always the same controls.** The four knobs always mean the same four things.
5. **Never touch the plumbing.** Loader, OTA/update, USB and HAL code stay as inherited.

## 2. Base

- **Fork:** Melodee (GPL-3.0, by Kerem Kilic, itself a Felucca derivative). Brings the PROPHET engine, FM6, dual-core build, lean engine memory, 808/909/SID drum engine, installer and host test suite (`tests/hostsim.c` renders the real DSP on a computer). It has **no** WASM browser build; Dock makes one from the host simulator.
- **Upstream:** Felucca by Leo Kuroshita / Hügelton Instruments (GPL-3.0). Source of WHEEL (organ), PHYS (physical models), ANALOG voice code, effects (chorus, delay, HALL/ROOM/SPRING reverb), FX layer, MIDI routing.
- Melodee 1.2.1 removed external effects to save voices. Dock needs effects back (they define each line-up's "room"), so bring them back from Felucca or Melodee 1.1.0, as lean as possible.
- **Licence:** Dock is GPL-3.0. Credit every source in the README and `LICENSES/`.
- **Reference only (no code):** Baud Girl FM-1+VA (closed source) for UI principles; JunoX and the GPL Juno 6 plugin for Juno behaviour; published Juno-60 chorus analyses.

## 3. The band

| Member | Default MIDI ch | Mode | Voices | Colour |
|---|---|---|---|---|
| **B — Bass** | 1 (Circuit Synth 1) | Mono, every note strikes, last-note priority, glide only on ties/slides | 1 | Violet `#A98BFF` |
| **K — Keys** | 2 (Circuit Synth 2) | Poly | 6 | Green `#46D39A` |
| **L — Lead** | 3 (Circuit MIDI track) | Mono, every note strikes, last-note priority, glide only on ties/slides | 1 | Amber `#FFB547` |

- Bass and Lead strike every note (Melodee's MONO), not legato: a legato overlap (a sequencer's tie) bent a ringing pluck instead of plucking again and skipped a bass's filter snap. A sound's glide moves only between overlapping notes.
- Last-note priority on Bass and Lead: a sequencer's slide sends the new note before the old note-off, so the new note must win (a Prophet's own low-note priority would lag rising lines).
- 8 voices total, **reserved per member** (user-adjustable, e.g. 2/5/1). Reserved voices are never stolen by another member.
- If a member exceeds its own share it steals from itself (oldest released note first).
- Under CPU overload, protect Bass and Lead first (SLOOP's rule).
- Channels configurable per member (1–16 or OFF). Channel 10 ignored by default (Circuit drums).
- Program Change on a member's channel loads its sound; Program Change on a global channel loads a Line-up (not built yet: Line-ups come with step 8). A member's Program Change picks entry **bank × 128 + program** of that member's list (Bank Select MSB, CC 0, defaults to 0): the browser's ALL list in order (all engines, one after another). HOME's strip counts within each engine instead ("JUNO 2 / 10"), so its number is not the program number; revisit with Line-ups.
- Sounds: Bass, Keys and Lead browse the same list (every engine but the drums), in the same order; the drummer (part 4) browses drum kits only.
- Thickness without voices: detuned dual oscillators, sub-osc, drive, chorus/ensemble, widener. Unison is optional and visibly costs voices.

## 4. Engines

| Engine | Status | Used for |
|---|---|---|
| **PROPHET** | From Melodee | Basses, brass, leads |
| **JUNO** | **New** | Pads, strings, silk leads, chorus I/II |
| **EP** | **New** (built: FRodhes' physical Rhodes) | Rhodes (tine + magnetic pickup bark), Wurlitzer (reed + electrostatic pickup), tremolo |
| **WHEEL** | From Felucca (built: back on) | Hammond + rotary; combo organs (Vox/Farfisa) from its registrations + vibrato |
| **PLUCK** | From Felucca PHYS (built: back on, all four models) | Upright-ish bass, surf/twang pluck |
| **DRUM** | Melodee's `eng_drum.c` to start (synthesised 808/909/SID); port Felucca's CR-78-style kits, congas and bells for the rhythm box (synthesised, no samples) | The rhythm box drummer (see §8b) |
| FM6 | **Decision open — leaning cut** | DX piano / bells. Possible later "Glass Dock" add-on |
| VOICE | **Cut** | — |

### JUNO engine spec
- One DCO per voice: saw, pulse with PWM (LFO or manual), sub square (-1 oct), noise; levels per source.
- HPF (4 fixed steps) → 4-pole low-pass, resonance, envelope amount (±), key tracking, LFO amount.
- One ADSR (shared by VCF/VCA, VCA can switch to gate), one LFO with delay.
- **Chorus I / II / I+II** — two modulated short delay lines, stereo. This is the sound; tune it carefully.
- BLEP/PolyBLEP oscillators; soft-saturating ZDF-style filter. Optional per-note Drift.

### EP engine spec
- Struck resonator per note (modal: fundamental + a few partials with tuned decay), hammer noise transient.
- Pickup stage: soft asymmetric saturation scaled by velocity → Rhodes bark. Wurly: brighter partials, harder odd-harmonic clip, nasal tone.
- Models: Suitcase, Stage, Wurly 200 (later: Clav-ish).
- Built-in stereo tremolo / auto-pan (Suitcase) and mono tremolo (Wurly).

**Built:** `firmware/src/eng_ep.c` is FRodhes' physically modelled Rhodes (Leonardo Mirabal, GPL-3.0: modal tine and
tone bar, measured-style hammer pulses, the electromagnetic pickup's flux as the bark, damper, pickup load and amp
body), its tables made by `tools/gen_rhodes.py`. TYPE: STAGE, SUITCASE, FELT, DYNO and a Wurly-ish voicing of the same
tine (WURLY: no reed / electrostatic model yet; an odd-harmonic clip and mono tremolo). TREM uses the part's LFO.
Flash cost: its tables are 144 KiB (`tests/ep_test.c` prints the breakdown); see CHANGELOG for what could shrink.

## 5. Line-ups (Docks)

Each Line-up stores all three sounds, voice split, macros **and its own effects ("room")**.

| Dock | Bass | Keys | Lead | Room |
|---|---|---|---|---|
| **Yacht** | Prophet funk bass, light drive | Suitcase Rhodes, stereo trem + chorus | Juno silk lead, delay | Warm plate |
| **Hot** (50s/60s) | Upright pluck, short and woody | Wurlitzer + trem | Surf twang pluck | Spring + slapback |
| **Kraut** | Prophet mono, squelchy filter env | Juno string-machine ensemble | Clean Juno square, glide | Tape echo, dry |
| **Goth** (Cure / shoegaze) | High chorused pick bass | Wide Juno string wash, slow attack | Flanged glassy lead, long delay | Huge shimmer |
| **Mod** (60s mods → The Jam / Style Council) | Round Motown bass | Hammond + rotary + drive | Vox/Farfisa combo organ | Small room, grit |

Power-on default: **Yacht Dock**.

**Built (first pass):** SELECT on HOME steps Yacht → Kraut → Hot → Mod → Goth and loads each one's four sounds (Bass, Keys, Lead, drum kit), best guesses from today's engines, in `firmware/lineups/` (built in as `firmware/src/dock_factory.h`). Mod's Keys and Lead are WHEEL (MOD HAMMOND, VOX COMBO), Hot's Bass and Lead PLUCK (UPRIGHT, SURF TWANG). Each stores its voice split (all 1/6/1 for now); still to come per Line-up: macros, groove.

**Built: the rooms.** Each Line-up sets Melodee's shared effects (three slots, CHAIN or SPLIT) and every part's sends
when it loads: Yacht warm plate + Lead delay; Hot spring (Felucca's, ported as a new effect) + slapback; Kraut dry with
a dark tape echo; Mod a small short room; Goth a huge drifting reverb, long delay and a chorus send (shimmer-ish: no
pitch shift yet). Host cost 0.35–1.2 of a PROPHET voice; buffers 4–14 KiB from the audio arena (CHANGELOG). Tempo, which SELECT set on Melodee's HOME, needs a new home.

**Built: your Line-ups.** A Line-up is one record of the whole band (`firmware/src/dock_lineups.c`): each part's
engine, preset, every parameter and its engine's own patch (level, pan, sends and inserts among them), the room and the
voice split, with room for macro offsets and a drum groove.
- **Hold SAVE on HOME** keeps the band as it is now as *your* version of the Line-up playing. SELECT then loads yours,
  and HOME's header marks it: a dot after the name ("Yacht • DOCK"). Power-on plays your Yacht if you saved one.
- **Back to factory:** HOME's sheet (OCT+ held, as every page's sheet) has *Save line-up* and *Factory line-up*. The
  second asks first ("Back to factory? Your saved Yacht goes"), then erases yours and plays the factory one. A sheet
  row, not a gesture: SAVE held is the save, and a step that loses something asks first (docs/design/README.md:
  dialogs for decisions).
- The five factory Line-ups can't be lost: they are built in, the same records as yours, made from the text files in
  `firmware/lineups/` (`tools/dock_lineup_import.py` writes `firmware/src/dock_factory.h`). To make an edited
  Line-up the real one: Export it on the emulator, run the tool on the file (emu/README.md).
- Yours live in the flash of Melodee's project slot D (no other flash was free): projects A–C as before.

## 6. Controls (FM-1 panel)

Dock has two jobs, and the panel serves both: **dialling in the right sound** (browse, edit, compare, save) and
**shaping it live while an external sequencer plays** (macros, mutes, momentary tweaks, performance effects).
Navigation follows Baud Girl's principles (concept only): **SELECT picks a parameter, ALGORITHM changes it, HOME
backs out.** The FM-1's knobs **only turn** (no push switch: the firmware reads 14 buttons and 7 knobs), so every
"press" is a button. Agreed map (2026-10-18); "today" is the firmware as it stands.

| Control | Dock (the plan) | Today | Job |
|---|---|---|---|
| **KNOB 1–4** | **Tone · Bite · Shape · Space** — always. PART = focused member; BAND = all members, relative to each part's own value (tap HOME on HOME to switch) | the member's four engine settings (CUT, RES, ENV…) | live |
| **SELECT** | On HOME: turn = change Line-up ✓. On pages: turn the pages | turn = Line-up ✓; on pages the pages ✓ (EDIT: the open section's) | both |
| **ALGORITHM** | Pick the focused member (on pages: change the picked parameter) | picks the member ✓ | both |
| **Black keys 1–4** (or held button + key) | Jump straight to Bass / Keys / Lead / Drums | play notes | live |
| **PRESETS** | Browse the focused member's sounds (engine strip, per-engine count) | ✓, on every page (ALL, RECENT, categories; no FAVES) | sound |
| **EDIT** | Deep engine pages for the focused member; **A/B**: compare your edit with the saved version | section list ✓ (KNOB 2 a section, OCT+ opens, SELECT its pages, OCT− back); hold = undo | sound |
| **ENV** | Focused member's envelope: page 1 the same on every engine (ATK DEC SUS REL, relative), then the engine's own | ✓ common page, then the engine's own pages, ADSR, ENV DEST | sound |
| **LFO** | Page 1 the same on every engine (RATE DEPTH, relative), then the engine's own, MOD last; **hold + turn a knob = momentary change, snaps back on release** | ✓ common page, engine's own, LFO WAVE, LFO SYNC, LFO DEST, MOD | both |
| **FX** | **Hold = performance FX layer** (§7: white keys the effects, black keys who goes through); double-tap = latch | lands on the room (SHARED) ✓, then the member's own slots | live |
| **GLO** | Tap = mixer (level, pan) ✓; **hold = mutes / solos on the black keys, tap tempo** | tap = mixer ✓; hold = mutes / solos B K L D, levels, tap tempo ✓ (Melodee's layer, relabelled) | live |
| **SAVE** | Tap = save sound; **hold on HOME = save Line-up** as yours ✓ (back to factory: HOME's sheet ✓; export: the emulator ✓) | Save as user; hold on HOME = save Line-up ✓; menu's Band: save, factory ✓ | sound |
| **ARP** | Arp on/off for focused member; knobs = rate, pattern, octaves, gate | ✓ (Melodee's) | live |
| **SCL** | Split / layer for the FM-1's own keys (tap a key for the split point) | nothing (scales removed) | sound |
| **SEQ** | Clock / thru status (source, incoming BPM); tempo's new home | CLOCK page ✓ (BPM, source, incoming BPM) | — |
| **REC** | Arm the looper for the focused member (notes, not audio); hold = clear member | nothing | live |
| **PLAY** | Start/stop looper + internal clock (ignored when an external clock leads) | internal clock ✓ | live |
| **HOME** | Stage view; **on HOME, tap = cycle PART → BAND → chord wheel** (the knobs shape one member, then the whole band, then Melodee's chord-wheel view, kept because it's fun); hold = menu (channels, voice split, clock, theme) | tap = chord wheel ✓; hold = menu ✓, with Band (line-up, voice split, channels) ✓ | both |
| **OCT− / OCT+** | Octave for focused member; both together = panic | ✓ (elsewhere: back / enter) | — |
| **MASTER** | Volume | ✓ | — |
| **Keys** | Play focused member, or the split/layer; lit in member colour; incoming MIDI notes flash | ✓ | — |

**Build order** (after saving): macros with PART/BAND → A/B compare → hold-GLO mutes → hold-LFO momentary tweaks →
hold-FX performance layer → black-key focus → SCL split → looper. Melodee's pages Dock doesn't need are off the panel
(docs/design/DOCK_SIMPLIFY.md, done 2026-10-18; SEQ's clock page with it); the common ENV / LFO pages (done
2026-10-18, below); next the macros.

**ENV and LFO, the same on every engine** (`firmware/src/dock_front.c`). Page 1 of each is a front door: ATK DEC SUS
REL, and RATE DEPTH, as offsets that nudge the sound's own envelopes and LFOs from where the sound has them (0 = the
sound as made; 16 steps = twice as long; levels toward the top or toward nothing). Prophet: both envelopes, its LFO;
FM6: every operator's EG, its LFO; CZ-1: the DCW and DCA envelopes, its vibrato; EP: DECAY, RELEASE, TREM; organ and
PLUCK: the ADSR plus the percussion's decay / the string's damping; JUNO, SID: the ADSR; a kit: DECY only. A control
with no meaning on an engine reads "--" (EP: ATK, SUS; a kit: all but DEC). The engine's own pages follow unchanged.
The offsets reset when a sound loads and are kept with a saved Line-up; the Shape macro will reuse the same wiring.

Knob meanings per engine:

| | Tone | Bite | Shape | Space |
|---|---|---|---|---|
| Juno | Cutoff | Resonance / PWM | Attack/Release | Chorus + verb |
| Prophet | Cutoff | Poly-mod / res | Filter env | Delay + verb |
| EP | Brightness | Bark | Decay | Trem + verb |
| Organ | Drawbar mix | Drive | Percussion | Rotary speed + verb |
| Pluck | Brightness | Pluck position | Decay | Verb |

## 7. Performance FX layer (hold FX)

- **Black keys:** which members go through (any combination).
- **White keys:** Stutter 1/8, Stutter 1/16, Reverse, Tape stop, Freeze, Filter sweep, Verb throw, Echo throw.
- **Knobs:** Depth, Mix, Rate, Gate.
- One shared FX bus (RAM is tight); ~1–2 s buffer. Synced to external clock, else internal tempo / tap.

## 8. Looper

Records **notes, not audio** (tiny memory; sounds can be changed after recording; macros and FX still apply).

- **BARS (default):** pick 1/2/4/8 bars → count-in (or start on first note) → records exactly that → closes on the downbeat.
- **FREE:** tap in / tap out, first loop sets length; end snaps to nearest beat.
- **CAPTURE:** always listening; REC keeps the last 1/2/4/8 bars already looping.
- Overdub per member; undo last pass; clear per member. On-screen bar counter, key LEDs pulse on the downbeat.
- Follows external clock when present.

## 8b. The drummer (rhythm box)

A preset rhythm box in the spirit of the Ace Tone, Maestro Rhythm King and organ drum machines: pick a groove, press play. Not a sequencer.

- **Sound:** synthesised kits: Melodee's DRUM engine now, Felucca's CR-78-style kits ported in (no flash cost). Optionally a few short one-shot samples per kit (e.g. brushed snare, tambourine) from the shared sample pool, kept small.
- **Voices:** runs in its own lightweight lane (part 4, MIDI ch 4), outside the 8 melodic voices: it never takes a band voice. Watch its CPU cost on the emulator meter.
- **Controls** (Drummer page, or a held-button layer): Play/stop · Groove select · Fill (tap for a fill into the next bar) · Variation/intensity knob (sparse → busy) · Swing · Level.
- **Clock:** follows external clock when present; otherwise internal tempo / tap.
- **Steps aside:** auto-mutes when notes arrive on channel 10 (the Circuit's drums). Option: channel-10 notes play Dock's drum sounds directly instead.
- **Each Line-up stores its kit and default groove.**

| Dock | Kit character | Grooves |
|---|---|---|
| Yacht | Soft, tight, rimshot, closed hats | Laid-back shuffle · Half-time · Light funk |
| Hot | Brushes, woody, small room | Twist · Surf · Rockabilly · Slow ballad (brushes) |
| Kraut | Dry, clipped, machine | Motorik · Pulse · Minimal |
| Goth | Floor toms, gated snare, drum machine | Cure-style tom groove · Slow gated · Driving 8ths |
| Mod | Bright snare, tambourine | Motown four-on-the-floor · Northern Soul stomp · Shuffle |

## 9. Visual design

**The style guide is [docs/design/DOCK_STYLE.md](docs/design/DOCK_STYLE.md)** (tokens, type, grid, components, the four
reference screens at 240 × 240). Summary:

- Fonts: **Space Grotesk** for words, **Unbounded** for big numbers and the DOCK wordmark, **Space Mono** for data.
  Sizes from Melodee's on-device "large type": 16 / 20 / 28 / 48 px, nothing smaller.
- Dark ground, generous space, calm: no hint rows, no tabs, nothing cut, nothing covering the picture.
- **Colour = band member** everywhere: strips, keys, LEDs, knob cards. Match Circuit colours where sensible.
- Four knob cards always visible; big value pops up while turning, then fades.
- Draw shapes (envelopes, filter, loop progress), not numbers.
- Home = three member strips (sound, engine, channel, activity, meter, loop state) + 8 voice squares coloured by owner.
- Mockups: the "Dock FM-1 Screens" design canvas (Home, Focused member, FX layer, Looper), translated to the square screen as `docs/design/dock/ref/` (`tools/dock_refs.py`).
- Drummer colour: bone `#E9DFC7` (proposed).

## 10. Phases

### Phase 1 — Browser emulator: get the feel and sounds right  ← START HERE
Goal: play Dock on screen with the Circuit plugged in, and tune sounds by ear. No hardware.

1. Clone Melodee; build a WASM version of its host simulator with the firmware DSP **unchanged** (prove it with the golden render hashes); confirm it plays from the Circuit over Web MIDI in Chrome.
2. Strip to three members (B/K/L), set default channels 1/2/3, hide sequencer pages.
3. Emulator page:
   - FM-1 panel on screen (mouse + computer keyboard), screen rendered at real resolution and scaled.
   - Web MIDI input select (Circuit), MIDI activity per member.
   - **Device-reality meters:** voices in use per member, and estimated device CPU load per audio block (use the repo's CPU-budget/regression tooling) so nothing sounds great in the browser but chokes on the FM-1.
   - A/B toggle and quick preset save/export to iterate sounds.
4. Voice reservation + mono/legato for Bass and Lead.
5. Four macros (Tone/Bite/Shape/Space), PART/BAND modes.
6. JUNO engine + chorus → tune against references.
7. EP engine (Rhodes, Wurly) + tremolo → tune. (Built from FRodhes; tuning by ear to do.)
8. Bring back lean effects; build the five Line-ups with their rooms. (Built: rooms on Melodee's shared effects, SPRING from Felucca; tuning by ear to do.)
9. New UI to the design language; Baud Girl-style navigation.
10. FX layer, then looper (BARS → CAPTURE → FREE).
11. The drummer: kits and grooves per Dock, fills, variation, channel-10 step-aside.

Exit criteria: you'd happily jam on it for an hour with the Circuit; all five Docks sound right; CPU estimate stays under budget with Keys playing 6-note chords.

### Phase 2 — Hardware build
- Install Docker + JieLi toolchain + AC79 SDK (per Felucca `BUILDING.md`); build `.fwsc`.
- Run the full host test suite (golden renders, CPU budgets).

### Phase 3 — First flash
- See safety rules. Flash a known release first, then Dock.

### Phase 4 — Polish and share
- README, manual, credits, web installer page; consider contacting Baud Girl and Melodee/Felucca authors.

## 11. Safety rules (non-negotiable)

1. Never modify loader, OTA/update entry, USB or HAL code.
2. Keep the update entry (`*_OTA`) enabled in every build; keep a rescue path (e.g. OCT− at power-on) and crash → safe mode.
3. Own a Seeed XIAO RP2040 + FM-1 Transporter set up **before** the first flash.
4. Practise by flashing an existing release (Felucca/Melodee) first; take a full backup.
5. Flash with full battery, a direct data cable, all other MIDI apps closed.
6. The person flashes the device — never an automated agent.
7. Offer a single-core build (some units' second core misreads memory).
8. Consider a second FM-1 as a dev unit.

## 12. Open questions

- Cut FM6? (Leaning yes.)
- Name settled: **Dock**, tagline **"Onboard a band."**
- Drummer: which button opens the Drummer page; a few one-shot samples or pure synthesis?
- Exact voice split defaults per Line-up.
- Undo gesture (SAVE-held is taken by "save Line-up"; back to a factory Line-up is a row of HOME's sheet).
- Split/layer UI details.
- Exact FM-1 screen resolution for the emulator.

## 13. Key links

- Melodee — github.com/keremimo/melodee
- Felucca — github.com/hugelton/Felucca (BUILDING.md)
- Dinghy (minimal base + host mode) — github.com/zvenson/dinghy
- FM-1 Transporter (recovery) — github.com/kurogedelic/FM-1-transporter
- awesome-fm-1 — github.com/cicloid/awesome-fm-1
- Firmware directory — altstrument.org/devices/m-vave-fm-1
- JunoX (Juno-60 reference) — github.com/pendragon-andyh/junox
- Baud Girl FM-1+VA (UI reference) — baudgirl.com/work/FM-1+VA
