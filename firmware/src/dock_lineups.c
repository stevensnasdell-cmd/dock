/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's Line-ups (DOCK.md §5): each a sound for Bass, Keys, Lead and the drummer, from the engines there are today
 * (best guesses, to tune by ear). SELECT on HOME steps through them in this order and loads all four (ui_dock.c dock_lineup_turn). Power-on is the first, Yacht, as engines.c TRK_DEF.
 * Still to come with them: each one's room (effects), voice split, macros and a drum groove. Included by ui_dock.c */
typedef struct {
    const char *name;                                   /* as HOME's header shows it */
    uint8_t snd[NPART][2];                              /* engine, preset: Bass, Keys, Lead, Drums */
} dock_lineup_t;

static const dock_lineup_t DOCK_LINEUPS[] = {
    {"Yacht", {{ENGI_PROPHET, 4},    /* Funk Bass II */
               {ENGI_EP, EP_SUITCASE}, /* SUITCASE: the Rhodes, stereo vibrato */
               {ENGI_JUNO, 1},       /* SILK LEAD */
               {ENGI_DRUM, 0}}},     /* 808 KIT */
    {"Kraut", {{ENGI_PROPHET, 21},   /* Bass Face: a squelchy mono */
               {ENGI_JUNO, 2},       /* STR MACHINE */
               {ENGI_JUNO, 5},       /* SQUARE LEAD */
               {ENGI_DRUM, 2}}},     /* SID KIT: dry, machine */
    {"Hot", {{ENGI_PHYS, PHYS_UPRIGHT}, /* PLUCK UPRIGHT: short and woody */
             {ENGI_EP, EP_WURLY},    /* WURLY: mono tremolo */
             {ENGI_PHYS, PHYS_SURF}, /* PLUCK SURF TWANG */
             {ENGI_DRUM, 0}}},       /* 808 KIT (brushes to come) */
    {"Mod", {{ENGI_CZ, 15},          /* ELEC.BASS 2: round, Motown */
             {ENGI_WHEEL, WHEEL_MOD_HAMMOND}, /* MOD HAMMOND: drawbars, rotary, drive */
             {ENGI_WHEEL, WHEEL_VOX}, /* VOX COMBO: the combo organ */
             {ENGI_DRUM, 1}}},       /* 909 KIT: a bright snare */
    {"Goth", {{ENGI_JUNO, 6},        /* JUNO BASS: chorused */
              {ENGI_JUNO, 3},        /* STRING WASH */
              {ENGI_JUNO, 9},        /* GLASS LEAD */
              {ENGI_DRUM, 3}}},      /* SID DIGI: a drum machine */
};
#define DOCK_NLINEUPS (sizeof DOCK_LINEUPS / sizeof DOCK_LINEUPS[0])

static uint8_t dock_lineup;                             /* the Line-up playing (power-on: Yacht) */

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
    ui.force = 1;
}
