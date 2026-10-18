/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's macros (firmware/src/dock_macro.c, HOME's KNOB 1-4: Tone, Bite, Shape, Space), on the emulator's firmware
 * build (emu/dock_web.c):
 *   - on every engine each macro changes the sound measurably (+40 or -40 against the sound as made)
 *   - Tone + is brighter than Tone - (more of the signal's energy in its changes, sample to sample); Shape + rings
 *     longer than Shape - (more of the first note's energy late in it, held and let go)
 *   - 0 is the sound as made: turned for a while and back to 0, the same render sample for sample as one that never
 *     moved (renders against the code before the macros existed were checked the same when it was written: CHANGELOG)
 *   - BAND (HOME tapped once): one turn moves Bass, Keys and Lead, each part's sound changes; Space the drummer's too
 *   - kept with a saved Line-up, 0 on a factory one and on a sound load, back with Undo
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -DMELODEE_USB_AUDIO=1 -Itests \
 *      -o build/host/dock_macro_test tests/dock_macro_test.c -lm */
#include "../emu/dock_web.c"
#include <sys/mman.h>
#include <sys/wait.h>

static int fails;
static void mcheck(const char *what, int ok)
{
    printf("macro: %-86s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_ms(uint32_t ms) { uint32_t e = fm1_ms + ms; while ((int32_t)(fm1_ms - e) < 0) web_render(128); }

/* a part's three notes (Keys: a chord), each 0.4 s held and 0.6 s let go: 3 s, mono sum */
#define NS (44100 * 3)
#define SLOT (NS / 3)
static void play(uint32_t k, float *o)
{
    static const uint8_t mel[3] = {48, 55, 60};
    uint32_t n = 0;
    for (int s = 0; s < 3; s++) {
        uint32_t note = k == DOCK_DRUMS ? (s == 1 ? 38u : 36u) : mel[s];
        web_midi(0x90 | k, note, 100);
        if (k == DOCK_KEYS) { web_midi(0x90 | k, note + 4u, 100); web_midi(0x90 | k, note + 7u, 100); }
        for (int b = 0; b < 138; b++) { web_render(128); for (int i = 0; i < 128; i++) o[n++] = out_l[i] + out_r[i]; }
        web_midi(0x80 | k, note, 0);
        if (k == DOCK_KEYS) { web_midi(0x80 | k, note + 4u, 0); web_midi(0x80 | k, note + 7u, 0); }
        for (int b = 0; b < 207; b++) { web_render(128); for (int i = 0; i < 128 && n < NS; i++) o[n++] = out_l[i] + out_r[i]; }
    }
}
/* renders from the state now (forked: each from the same state) with part k's macros set to m */
static float *shm;
static void render_at(uint32_t k, int slot, const int16_t *m)
{
    pid_t p = fork();
    if (!p) {
        memcpy(dock_macro[k], m, sizeof dock_macro[k]);
        run_ms(100);                                    /* (a block for the caches; the same for every take) */
        play(k, shm + (size_t)slot * NS);
        _exit(0);
    }
    waitpid(p, 0, 0);
}
static double diff_db(int a, int b)                      /* the difference against render a, dB (-200: none) */
{
    const float *x = shm + (size_t)a * NS, *y = shm + (size_t)b * NS;
    double e = 0, d = 0;
    for (int i = 0; i < NS; i++) { e += (double)x[i] * x[i]; d += (double)(x[i] - y[i]) * (x[i] - y[i]); }
    return d > 0 ? 10 * log10(d / (e + 1e-20)) : -200;
}
static int same(int a, int b) { return !memcmp(shm + (size_t)a * NS, shm + (size_t)b * NS, NS * sizeof(float)); }
static double bright(int a)                              /* energy of the first difference / energy (dB) */
{
    const float *x = shm + (size_t)a * NS;
    double e = 0, d = 0;
    for (int i = 1; i < NS; i++) { e += (double)x[i] * x[i]; d += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]); }
    return 10 * log10((d + 1e-20) / (e + 1e-20));
}
static double ratio(int a, int a0, int a1, int b0, int b1)   /* the first note: energy of the signal's changes in
                                                             * samples b0..b1 / a0..a1 (dB) */
{
    const float *x = shm + (size_t)a * NS;
    double e0 = 0, e1 = 0;
    for (int i = 1; i < SLOT; i++) {
        double v = (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        if (i >= a0 && i < a1) e0 += v;
        if (i >= b0 && i < b1) e1 += v;
    }
    return 10 * log10((e1 + 1e-20) / (e0 + 1e-20));
}
/* how long the first note rings: while held, 0.1-0.4 s against its first 50 ms (a decay), plus the release,
 * 0.45-1 s against the note held (a tail); the signal's changes, so a high partial's decay (the organ's
 * percussion) counts as much as the body's */
static double rings(int a) { return ratio(a, 0, 2205, 4410, 17640) + ratio(a, 0, 17640, 19845, SLOT); }

typedef struct { const char *name; uint32_t part, engine, preset; } sound_t;
static const sound_t SOUNDS[] = {
    {"PROPHET Feature My Bass", DOCK_BASS, ENGI_PROPHET, 135}, {"PROPHET Faint Memories", DOCK_LEAD, ENGI_PROPHET, 127},
    {"FM6 4", DOCK_KEYS, ENGI_FM6, 4}, {"CZ-1 ELEC.PIANO", DOCK_LEAD, ENGI_CZ, 28}, {"SID 1", DOCK_LEAD, 3, 1},
    {"JUNO STR MACHINE", DOCK_KEYS, ENGI_JUNO, 2}, {"JUNO SILK LEAD", DOCK_LEAD, ENGI_JUNO, 1},
    {"EP SUITCASE", DOCK_KEYS, ENGI_EP, 1}, {"organ MOD HAMMOND", DOCK_KEYS, ENGI_WHEEL, 5},
    {"PLUCK UPRIGHT", DOCK_BASS, ENGI_PHYS, 9}, {"PLUCK SURF TWANG", DOCK_LEAD, ENGI_PHYS, 10},
    {"DRUM 808 KIT", DOCK_DRUMS, ENGI_DRUM, 0}, {"DRUM 909 KIT", DOCK_DRUMS, ENGI_DRUM, 1},
};
static const char *const MC_NAMES[MC_N] = {"Tone", "Bite", "Shape", "Space"};

static void load(const sound_t *s)
{
    dock_lineup_load(0);                                /* (Yacht's room: the sends as the band has them) */
    run_ms(200);
    undo_depth++;
    engine_load_of(&trk[s->part], s->engine, s->preset);
    undo_depth--;
    song.sel = s->part;
    run_ms(400);
}

/* each macro moves the sound; Tone + brighter, Shape + longer; 0 again is the sound as made */
static void macros(const sound_t *s)
{
    char what[140];
    load(s);
    for (uint32_t k = 0; k < MC_N; k++) {
        int16_t m[MC_N] = {0};
        render_at(s->part, 0, m);
        m[k] = -40;
        render_at(s->part, 1, m);
        m[k] = 40;
        render_at(s->part, 2, m);
        double d = diff_db(0, 1) > diff_db(0, 2) ? diff_db(0, 1) : diff_db(0, 2);
        snprintf(what, sizeof what, "%s: %s moves it (-40 %.0f dB, +40 %.0f dB)", s->name, MC_NAMES[k], diff_db(0, 1),
                 diff_db(0, 2));
        mcheck(what, d > -40.0);
        if (k == MC_TONE) {
            snprintf(what, sizeof what, "%s: Tone + brighter (%.1f < %.1f < %.1f dB)", s->name, bright(1), bright(0),
                     bright(2));
            mcheck(what, bright(1) < bright(2));
        }
        if (k == MC_SHAPE && s->engine == ENGI_WHEEL) {   /* the organ: its percussion's decay (a quiet partial
                                                          * under the bars: the part's own figure, not the mix's) */
            uint32_t pd[3];
            for (int c = 0; c < 3; c++) {
                dock_macro[s->part][MC_SHAPE] = (int16_t)(c ? c == 1 ? -40 : 40 : 0);
                run_ms(10);
                pd[c] = drw_of(&trk[s->part])->t.pdec;
            }
            dock_macro[s->part][MC_SHAPE] = 0;
            snprintf(what, sizeof what, "%s: Shape + its percussion longer (a block's decay %u < %u < %u / 65536)", s->name,
                     pd[1], pd[0], pd[2]);
            mcheck(what, pd[1] < pd[0] && pd[0] < pd[2]);
        } else if (k == MC_SHAPE) {
            snprintf(what, sizeof what, "%s: Shape + rings longer (%.1f < %.1f < %.1f dB)", s->name, rings(1), rings(0),
                     rings(2));
            mcheck(what, rings(1) < rings(2));
        }
    }
    {   /* 0 again: every macro turned for a while, then back: the next render as one from the same moment that never
         * left 0 (the engines' caches made again). (Turned between notes: a note played with them leaves its echo in
         * the room; the organ's Space left alone, its rotor would have sped up and turned on further) */
        pid_t p;
        for (int c = 0; c < 2; c++)
            if (!(p = fork())) {
                for (uint32_t k = 0; k < MC_N; k++)
                    dock_macro[s->part][k] = c && !(k == MC_SPACE && s->engine == ENGI_WHEEL) ? (k & 1u ? -40 : 40) : 0;
                run_ms(300);
                memset(dock_macro[s->part], 0, sizeof dock_macro[s->part]);
                play(s->part, shm + (size_t)c * NS);
                _exit(0);
            } else
                waitpid(p, 0, 0);
        snprintf(what, sizeof what, "%s: at 0, turned and back, the sound as made", s->name);
        mcheck(what, same(0, 1));
    }
}

/* BAND: HOME tapped once, a turn moves Bass, Keys and Lead (each part's sound changes), Space the drummer's too */
static void band(void)
{
    char what[140];
    int16_t keep[NPART][MC_N];
    dock_lineup_load(0);
    run_ms(300);
    go_home();
    dock_macro_band = 0;
    dock_home_tap();
    mcheck("HOME tapped on HOME: BAND", dock_macro_band && !ui.home_pg);
    dock_macro_turn(MC_TONE, 15);
    dock_macro_turn(MC_SPACE, 15);
    memcpy(keep, dock_macro, sizeof keep);
    for (uint32_t k = 0; k < NPART; k++) {
        int16_t z[MC_N] = {0};
        render_at(k, 0, z);
        render_at(k, 1, keep[k]);
        snprintf(what, sizeof what, "BAND, Tone and Space turned: %s changes (%.0f dB; Tone %d, Space %d)",
                 k == DOCK_DRUMS ? "the drummer (Space)" : k == DOCK_BASS ? "the Bass" : k == DOCK_KEYS ? "the Keys" : "the Lead",
                 diff_db(0, 1), keep[k][MC_TONE], keep[k][MC_SPACE]);
        mcheck(what, diff_db(0, 1) > -40.0 && keep[k][MC_SPACE] == 30 && keep[k][MC_TONE] == (k == DOCK_DRUMS ? 0 : 30));
    }
    dock_home_tap();
    mcheck("  again: the chord wheel (the knobs still the band's)", ui.home_pg && dock_macro_band);
    dock_home_tap();
    mcheck("  again: PART", !ui.home_pg && !dock_macro_band);
    memset(dock_macro, 0, sizeof dock_macro);
}

/* kept with a saved Line-up; 0 on a factory one and on a sound load; back with Undo */
static void saving(void)
{
    static dock_lineup_t r;
    int ok = 1;
    dock_lineup_load(0);
    run_ms(200);
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < MC_N; j++) dock_macro[k][j] = (int16_t)((int)(k * 11 + j * 5) - 20);
    dock_lineup_capture(&r, 0);
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < MC_N; j++) ok &= r.macro[k][j] == (int8_t)((int)(k * 11 + j * 5) - 20);
    mcheck("a Line-up kept: every part's macros in its record", ok);
    dock_lineup_apply(&DOCK_LINEUPS[0]);
    ok = 1;
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < MC_N; j++) ok &= dock_macro[k][j] == 0;
    mcheck("a factory Line-up: all at 0", ok);
    dock_lineup_apply(&r);
    ok = 1;
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < MC_N; j++) ok &= dock_macro[k][j] == r.macro[k][j];
    mcheck("the kept one again: its macros back", ok);
    {   /* the same render as the band had when it was kept */
        pid_t p;
        for (int c = 0; c < 2; c++)
            if (!(p = fork())) {
                if (c)                                  /* (the same time both ways: the LFOs run on) */
                    dock_lineup_apply(&DOCK_LINEUPS[0]);
                run_ms(200);
                dock_lineup_apply(&r);
                run_ms(300);
                play(DOCK_KEYS, shm + (size_t)c * NS);
                _exit(0);
            } else
                waitpid(p, 0, 0);
        mcheck("  and it sounds as it did (the Keys, sample for sample)", same(0, 1));
    }
    undo_clear();
    track_select(DOCK_KEYS);
    apply_preset_to(&trk[DOCK_KEYS], 2);
    mcheck("a sound load: that part's at 0, browsing starts neutral (the others stay)",
           !dock_macro[DOCK_KEYS][MC_TONE] && !dock_macro[DOCK_KEYS][MC_SPACE] &&
           dock_macro[DOCK_LEAD][MC_TONE] == r.macro[DOCK_LEAD][MC_TONE]);
    undo_step(0);
    ok = 1;
    for (uint32_t j = 0; j < MC_N; j++) ok &= dock_macro[DOCK_KEYS][j] == r.macro[DOCK_KEYS][j];
    mcheck("Undo: the sound and its macros back", ok);
    memset(dock_macro, 0, sizeof dock_macro);
}

int main(void)
{
    shm = mmap(0, (size_t)NS * 4 * 4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    web_boot();
    run_ms(300);
    for (uint32_t i = 0; i < NELEM(SOUNDS); i++)
        macros(&SOUNDS[i]);
    band();
    saving();
    printf(fails ? "macro: %d FAILED\n" : "macro: all passed\n", fails);
    return fails != 0;
}
