/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's Line-ups (DOCK.md §5): the whole band in one record, a factory one or yours. SELECT on HOME steps through
 * them in DOCK_LINEUPS' order and loads all four parts (ui_dock.c dock_lineup_turn); power-on loads the first,
 * Yacht (its sounds are engines.c TRK_DEF's).
 *
 * A record (dock_lineup_t) is the band's full state: each part's engine, preset, every track parameter (t->p: the
 * sound, every edit, level, pan, the inserts and the effect sends) and its engine's own patch (PROPHET, CZ-1, FM6 with
 * its function settings, DRUM; JUNO, EP, WHEEL and PLUCK keep theirs in t->p), the room (Melodee's shared effects,
 * fx.c: three slots, CHAIN or SPLIT, the return) and the voice split (voice.c dock_share), with room for the macros'
 * offsets and a drum groove. The factory five are such records, made from text files (firmware/lineups/*.dock,
 * tools/dock_lineup_import.py -> dock_factory.h), so a Line-up saved on the emulator can become a factory one exactly
 * as it sounds.
 *
 * Yours: hold SAVE on HOME keeps the band as it is now as your version of the Line-up playing (dock_store.c: in the
 * flash of Melodee's project slot D); SELECT then loads yours, and HOME's header marks it with a dot. HOME's sheet
 * (OCT+ held) has Save line-up and Factory line-up (back to the factory one: yours goes, after a question).
 * Included by ui_dock.c */
#define DOCK_NAME 8u                                    /* a Line-up's name, 0-padded: "Yacht" (case as shown) */
#define DOCK_PATCH 144u                                 /* the largest engine patch: CZ-1's, FM6 packed + its settings */
typedef struct { uint8_t type, mix, rate, chr; int8_t tone; } dock_fx_t;   /* a shared slot: fx.c FX_*, as the FX page */
typedef struct {
    uint8_t mode;                                       /* G_FXMODE: 0 CHAIN (one send, slots in series), 1 SPLIT */
    uint8_t ret;                                        /* G_FXRETURN */
    dock_fx_t fx[FX_SLOTS];
} dock_room_t;
typedef struct {
    uint8_t engine, preset;                             /* (the preset: the name HOME shows; the sound is below) */
    int16_t p[P_COUNT];                                 /* every track parameter, as stored (motion's base) */
    uint8_t patch[DOCK_PATCH];                          /* the engine's own patch (dock_patch_size), zero after it */
} dock_part_t;
typedef struct {
    char name[DOCK_NAME];
    uint8_t share[NPART];                               /* the voices: Bass, Keys, Lead (eight), the drummer's lane */
    dock_room_t room;
    int8_t macro[NPART][4];                             /* (to come) each part's Tone, Bite, Shape, Space offset: 0 */
    uint8_t groove[4];                                  /* (to come) the drummer's groove: 0 */
    dock_part_t part[NPART];                            /* Bass, Keys, Lead, Drums */
    int8_t front[NPART][8];                             /* each part's common ENV / LFO offsets (dock_front.c: ATK DEC
                                                         * SUS REL RATE DEPTH; 2 spare): 0 the sound as made. Last, so
                                                         * a record of before them is this one's start (dock_store.c) */
} dock_lineup_t;
#define DOCK_LINEUP_V1 (sizeof(dock_lineup_t) - sizeof(((dock_lineup_t *)0)->front))   /* (a record without them) */

#include "dock_factory.h"                               /* DOCK_LINEUPS: generated from firmware/lineups/ */
#define DOCK_NLINEUPS (sizeof DOCK_LINEUPS / sizeof DOCK_LINEUPS[0])

static uint8_t dock_lineup;                             /* the Line-up playing (power-on: Yacht) */
static uint8_t dock_mine;                               /* bit i: Line-up i is yours (saved): SELECT loads that */

static const dock_lineup_t *dock_store_get(const char *name);   /* dock_store.c: your version (0: none) */
static int dock_store_put(uint32_t i);                  /* .. the band now as yours of Line-up i: 0 kept */
static int dock_store_drop(const char *name);           /* .. yours gone: 0 done */

/* the bytes of engine e's own patch in a record's part */
static uint32_t dock_patch_size(uint32_t e)
{
    return e == ENGI_PROPHET ? sizeof(p5_patch_t) : e == ENGI_CZ ? sizeof(cz_patch_t) :
           e == ENGI_FM6 ? FM6_PACKED + FM6_NFN : e == ENGI_DRUM ? sizeof(drum_patch_t) : 0u;
}
_Static_assert(sizeof(p5_patch_t) <= DOCK_PATCH && sizeof(cz_patch_t) <= DOCK_PATCH &&
               FM6_PACKED + FM6_NFN <= DOCK_PATCH && sizeof(drum_patch_t) <= DOCK_PATCH, "an engine's patch fits");

/* the band as it is now -> r, named as Line-up i */
static void dock_lineup_capture(dock_lineup_t *r, uint32_t i)
{
    uint32_t k, j, s;
    memset(r, 0, sizeof *r);
    str_cpy(r->name, DOCK_LINEUPS[i % DOCK_NLINEUPS].name, DOCK_NAME);
    for (k = 0; k < NPART; k++)
#if DOCK_VOICES
        r->share[k] = dock_share[k];
#else
        r->share[k] = DOCK_LINEUPS[0].share[k];
#endif
    r->room.mode = (uint8_t)song.g[G_FXMODE];
    r->room.ret = (uint8_t)song.g[G_FXRETURN];
    for (s = 0; s < FX_SLOTS; s++) {
        const int16_t *g = song.g + G_MFXTYPE + s * FX_PARAMS;
        r->room.fx[s].type = (uint8_t)g[0];
        r->room.fx[s].mix = (uint8_t)g[1];
        r->room.fx[s].rate = (uint8_t)g[2];
        r->room.fx[s].chr = (uint8_t)g[3];
        r->room.fx[s].tone = (int8_t)song.g[G_MFXTONE + s];
    }
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        dock_part_t *d = &r->part[k];
        d->engine = t->eng_req;
        d->preset = t->preset;
        for (j = 0; j < P_COUNT; j++)
            d->p[j] = motion_base_value(t, j);
        if (t->eng_req == ENGI_PROPHET)
            memcpy(d->patch, p5_patch_of(t), sizeof(p5_patch_t));
        else if (t->eng_req == ENGI_CZ)
            memcpy(d->patch, &cz_patch[k], sizeof(cz_patch_t));
        else if (t->eng_req == ENGI_FM6) {
            fm6_pack(fm6_patch[k], d->patch);
            memcpy(d->patch + FM6_PACKED, fm6_fn[k], FM6_NFN);
        } else if (t->eng_req == ENGI_DRUM)
            memcpy(d->patch, &drum_patch[k], sizeof(drum_patch_t));
        for (j = 0; j < FR_N; j++)
            r->front[k][j] = (int8_t)dock_front[k][j];
    }
}

/* a room -> the shared effects (a change of type or routing fades the old one out first, fx.c: no click) */
static void dock_room_apply(const dock_room_t *r)
{
    uint32_t s;
    song.g[G_FXMODE] = (int16_t)clamp(r->mode, GP[G_FXMODE].min, GP[G_FXMODE].max);
    song.g[G_FXRETURN] = (int16_t)clamp(r->ret, GP[G_FXRETURN].min, GP[G_FXRETURN].max);
    for (s = 0; s < FX_SLOTS; s++) {
        int16_t *g = song.g + G_MFXTYPE + s * FX_PARAMS;
        const dock_fx_t *f = &r->fx[s];
        g[0] = (int16_t)clamp(f->type, GP[G_MFXTYPE].min, GP[G_MFXTYPE].max);
        g[1] = f->mix;
        g[2] = f->rate;
        g[3] = f->chr;
        song.g[G_MFXTONE + s] = (int16_t)clamp(f->tone, -FX_TONE_MAX, FX_TONE_MAX);
    }
}

/* r -> the band: every part's sound as a load does it (one undo level each; the notes sounding released; its sends
 * and inserts with it), the room, the voice split. A part whose engine it may not play (a damaged record) stays */
static void dock_lineup_apply(const dock_lineup_t *r)
{
    uint32_t k, i, sum = 0;
    dock_room_apply(&r->room);
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        const dock_part_t *d = &r->part[k];
        uint32_t e = d->engine;
        if (e >= NENGINES || !ENGINES[e]->npresets || !DOCK_ENGINE_OK(k, e))
            continue;
        load_begin(t, UNDO_SOUND);
        panic_req |= (uint8_t)(1u << k);                /* (the sound changes under them) */
        fm1_irq_off();                                  /* the audio ISR sees the old sound or the new, whole */
        t->eng_req = (uint8_t)e;
        t->user = 0;
        t->user_native = 0;
        t->preset = (uint8_t)(d->preset % ENGINES[e]->npresets);
        for (i = 0; i < P_COUNT; i++)                   /* exactly as kept, not clamped as a project's load does:
                                                         * a preset may sit past its parameter's range (JUNO SQUARE
                                                         * LEAD's AMT4 64) and must sound as it did */
            t->p[i] = param_reserved(i) ? 0 : d->p[i];
        memset(&drum_patch[k], 0, sizeof(drum_patch_t));   /* (as a preset load: a kit's own edits only) */
        for (i = 0; i < FR_N; i++)                      /* the common ENV / LFO offsets (a factory one's: 0) */
            dock_front[k][i] = (int16_t)clamp(r->front[k][i], FR_MIN, FR_MAX);
        if (e == ENGI_PROPHET) {
            memcpy(&p5_patch[k], d->patch, sizeof(p5_patch_t));
            if (!p5_patch_valid(&p5_patch[k]))
                p5_patch_init(&p5_patch[k]);
            p5_ready[k] = 1;
        } else if (e == ENGI_CZ) {
            memcpy(&cz_patch[k], d->patch, sizeof(cz_patch_t));
            if (!cz_patch_valid(cz_patch[k].raw))
                cz_patch_init(cz_patch[k].raw);
        } else if (e == ENGI_DRUM) {
            memcpy(&drum_patch[k], d->patch, sizeof(drum_patch_t));
            if (!drum_patch_valid(&drum_patch[k]))
                memset(&drum_patch[k], 0, sizeof(drum_patch_t));
        }
        cz_track_accept(t);
        fm1_irq_on();
        if (e == ENGI_FM6) {                            /* (its patch arrives as a load: the notes stop) */
            uint8_t v[FP_SIZE + 1u];
            fm6_unpack(d->patch, v);
            fm6_put_patch(k, v, 1);
            memcpy(fm6_fn[k], fm6_fn_ok(d->patch + FM6_PACKED) ? d->patch + FM6_PACKED : FM6_FNDEF, FM6_NFN);
        }
        if (t == TSEL)
            sync_reload = 1;
        load_end(t);
    }
#if DOCK_VOICES
    for (k = 0; k < 3u; k++)
        sum += r->share[k] >= 1u ? r->share[k] : 8u;
    if (sum <= 8u)                                      /* (each member one at least, eight in all) */
        for (k = 0; k < 3u; k++)
            dock_share[k] = r->share[k];
#else
    (void)sum;
#endif
}

/* load Line-up i: yours when you saved one, else the factory one */
static void dock_lineup_load(uint32_t i)
{
    const dock_lineup_t *mine;
    dock_lineup = (uint8_t)(i % DOCK_NLINEUPS);
    mine = dock_store_get(DOCK_LINEUPS[dock_lineup].name);
    if (mine)
        dock_mine |= (uint8_t)(1u << dock_lineup);
    else
        dock_mine &= (uint8_t)~(1u << dock_lineup);
    dock_lineup_apply(mine ? mine : &DOCK_LINEUPS[dock_lineup]);
    ui.force = 1;
}

/* "YACHT " + what: a message about the Line-up playing */
static void dock_lineup_say(const char *what)
{
    char b[DOCK_NAME + 1u];
    uint32_t i;
    for (i = 0; DOCK_LINEUPS[dock_lineup].name[i] && i < DOCK_NAME; i++) {
        char c = DOCK_LINEUPS[dock_lineup].name[i];
        b[i] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
    }
    b[i++] = ' ';
    b[i] = 0;
    ui_say(b, what);
}

/* hold SAVE on HOME, the sheet's Save line-up: the band now as your version of the Line-up playing */
static void dock_lineup_save(void)
{
    int rc;
    if (transport_busy()) {                             /* (a flash erase stalls the audio: as a project's save) */
        ui_message("STOP TO SAVE");
        return;
    }
    momentary_restore();                                /* (an LFO-held change is not the sound) */
    rc = dock_store_put(dock_lineup);
    if (rc) {
        ui_message(rc == 3 ? "LINE-UPS FULL" : "SAVE ERROR");
        return;
    }
    dock_mine |= (uint8_t)(1u << dock_lineup);
    dock_lineup_say(MELODEE_FLASH ? "SAVED" : "SAVED (RAM)");
    ui.force = 1;
}

/* the sheet's Factory line-up, after its question: yours goes, the factory one plays */
static void dock_lineup_factory(void)
{
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    if (dock_store_drop(DOCK_LINEUPS[dock_lineup].name)) {
        ui_message("SAVE ERROR");
        return;
    }
    dock_lineup_load(dock_lineup);
    dock_lineup_say("FACTORY");
}
