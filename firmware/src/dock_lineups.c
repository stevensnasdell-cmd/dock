/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's Line-ups (DOCK.md §5): each a sound for Bass, Keys, Lead and the drummer, from the engines there are today
 * (best guesses, to tune by ear). SELECT on HOME steps through them in this order and loads all four (ui_dock.c dock_lineup_turn). Power-on is the first, Yacht, as engines.c TRK_DEF.
 * Each also brings its room (DOCK.md §5): Melodee's shared effects (fx.c: three slots, CHAIN or SPLIT, measured on the
 * device) set up for it, and every part's sends; the parts' own inserts are cleared. Still to come: voice split, macros
 * and a drum groove. Included by ui_dock.c */
typedef struct { uint8_t type, mix, rate, chr; int8_t tone; } dock_fx_t;   /* a shared slot: fx.c FX_*, as the FX page */
typedef struct {
    uint8_t mode;                                       /* G_FXMODE: 0 CHAIN (one send, slots in series), 1 SPLIT */
    uint8_t ret;                                        /* G_FXRETURN */
    dock_fx_t fx[FX_SLOTS];
    uint8_t send[NPART][FX_SLOTS];                      /* each part's SEND, SEND 2, SEND 3 (CHAIN: only the first) */
} dock_room_t;
typedef struct {
    const char *name;                                   /* as HOME's header shows it */
    uint8_t snd[NPART][2];                              /* engine, preset: Bass, Keys, Lead, Drums */
    dock_room_t room;
} dock_lineup_t;

/* ECHO's TIME is 10 + 2.8 ms a step (fx.c fx_time: 110 + 31 x RATE samples at FS/4), its REPEAT a feedback of
 * CHAR / 149; REVERB's DEPTH sets its tail, 0.3 s + 5.7 s x (CHAR / 127)^2; SPRING's DECAY the loop's gain */
#define DOCK_ECHO_MS(ms) ((uint8_t)((((ms) * 11025 / 1000) - 110) / 31))

static const dock_lineup_t DOCK_LINEUPS[] = {
    {"Yacht", {{ENGI_PROPHET, 135},  /* Feature My Bass: in tune, round (Funk Bass II is a detuned, synced program:
                                      * ~65 cents sharp by design, a chiptune edge) */
               {ENGI_EP, EP_SUITCASE}, /* SUITCASE: the Rhodes, stereo vibrato */
               {ENGI_JUNO, 1},       /* SILK LEAD */
               {ENGI_DRUM, 0}},      /* 808 KIT */
     /* warm plate: a short dense tank, its top rolled off; the Lead's own delay (300 ms: an eighth at 100 BPM) */
     {1, 100, {{FX_REVERB, 127, 48, 66, -12}, {FX_ECHO, 127, DOCK_ECHO_MS(300), 55, -20}, {0}},
      {{10, 0, 0}, {50, 0, 0}, {40, 45, 0}, {22, 0, 0}}}},
    {"Kraut", {{ENGI_PROPHET, 107},  /* Rubber Bumper: a rubbery filter snap (picked by ear, 2026-10-17) */
               {ENGI_JUNO, 2},       /* STR MACHINE */
               {ENGI_JUNO, 5},       /* SQUARE LEAD */
               {ENGI_DRUM, 2}},      /* SID KIT: dry, machine */
     /* tape echo and dry: no reverb; the echo's repeats through TAPE's drifting filters, dark, in series (CHAIN) */
     {0, 100, {{FX_ECHO, 127, DOCK_ECHO_MS(340), 80, -28}, {FX_TAPE, 127, 40, 60, 0}, {0}},
      {{0, 0, 0}, {28, 0, 0}, {50, 0, 0}, {0, 0, 0}}}},
    {"Hot", {{ENGI_PHYS, PHYS_UPRIGHT}, /* PLUCK UPRIGHT: short and woody */
             {ENGI_EP, EP_WURLY},    /* WURLY: mono tremolo */
             {ENGI_PHYS, PHYS_SURF}, /* PLUCK SURF TWANG */
             {ENGI_DRUM, 0}},        /* 808 KIT (brushes to come) */
     /* spring + slapback: SPRING (fx.c, after Felucca's), a single 110 ms repeat; the twang soaks in both */
     {1, 100, {{FX_SPRING, 127, 40, 112, 0}, {FX_ECHO, 127, DOCK_ECHO_MS(110), 0, -8}, {0}},
      {{18, 16, 0}, {50, 25, 0}, {72, 55, 0}, {26, 0, 0}}}},
    {"Mod", {{ENGI_CZ, 14},          /* ELEC.BASS 1: a fingered electric, Motown (picked by ear) */
             {ENGI_WHEEL, WHEEL_MOD_HAMMOND}, /* MOD HAMMOND: drawbars, rotary, drive */
             {ENGI_WHEEL, WHEEL_VOX}, /* VOX COMBO: the combo organ */
             {ENGI_DRUM, 1}},        /* 909 KIT: a bright snare */
     /* small room, grit: REVERB at its smallest and short (0.6 s; denser than ROOM's four combs, which the organ's own
      * tail hid), sent hard (the grit: the organ's own DRV) */
     {1, 100, {{FX_REVERB, 127, 8, 44, 6}, {0}, {0}},
      {{18, 0, 0}, {62, 0, 0}, {56, 0, 0}, {52, 0, 0}}}},
    {"Goth", {{ENGI_PROPHET, 19},    /* Sticky Plucks: a high, picked bass (picked by ear) */
              {ENGI_JUNO, 3},        /* STRING WASH */
              {ENGI_PROPHET, 127},   /* Faint Memories: glassy, soft (picked by ear) */
              {ENGI_DRUM, 3}},       /* SID DIGI: a drum machine */
     /* huge wash (shimmer-ish): REVERB at full size and depth (its lines drift), a long delay, and a deep slow
      * CHORUS send that widens the Bass and the Lead (no pitch-shifted shimmer: that needs another buffer) */
     {1, 100, {{FX_REVERB, 127, 127, 124, 0}, {FX_ECHO, 127, DOCK_ECHO_MS(367), 100, -16}, {FX_CHORUS, 127, 18, 110, 0}},
      {{24, 0, 60}, {80, 22, 40}, {66, 66, 60}, {40, 0, 0}}}},
};
#define DOCK_NLINEUPS (sizeof DOCK_LINEUPS / sizeof DOCK_LINEUPS[0])

static uint8_t dock_lineup;                             /* the Line-up playing (power-on: Yacht) */

/* set Line-up i's room: the shared slots, the return, the routing and every part's sends; the inserts off. A change
 * of type or routing fades the old one out first (fx.c), so a Line-up change never clicks */
static void dock_room_load(uint32_t i)
{
    const dock_room_t *r = &DOCK_LINEUPS[i % DOCK_NLINEUPS].room;
    uint32_t k, s;
    song.g[G_FXMODE] = r->mode;
    song.g[G_FXRETURN] = r->ret;
    for (s = 0; s < FX_SLOTS; s++) {
        int16_t *g = song.g + G_MFXTYPE + s * FX_PARAMS;
        g[0] = r->fx[s].type;
        g[1] = r->fx[s].mix;
        g[2] = r->fx[s].rate;
        g[3] = r->fx[s].chr;
        song.g[G_MFXTONE + s] = r->fx[s].tone;
    }
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        for (s = 0; s < FX_SLOTS; s++) {
            t->p[P_FXSEND + s] = r->send[k][s];
            t->p[P_FXTYPE + s * FX_PARAMS] = 0;
            t->p[P_FXTONE + s] = 0;
        }
    }
}

/* load Line-up i's four sounds onto the parts (Bass and Lead turn mono legato as any load does, dock.h) */
static void dock_lineup_load(uint32_t i)
{
    uint32_t k;
    dock_lineup = (uint8_t)(i % DOCK_NLINEUPS);
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        uint32_t e = DOCK_LINEUPS[dock_lineup].snd[k][0], p = DOCK_LINEUPS[dock_lineup].snd[k][1];
        if (t->eng_req != e)
            set_engine_of(t, e);
        apply_preset_to(t, p);
    }
    dock_room_load(dock_lineup);
    ui.force = 1;
}
