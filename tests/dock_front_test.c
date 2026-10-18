/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's common ENV and LFO (firmware/src/dock_front.c), on the emulator's firmware build (emu/dock_web.c):
 *   - on every engine each control that means something there changes the sound measurably, both ways summed (RATE
 *     with an LFO going: DEPTH +32 under both renders); the ones that mean nothing (EP: ATK SUS; a kit: all but DEC)
 *     are greyed and do nothing
 *   - "+" is longer: a release (REL) and a decay (DEC) last longer at +32 than at 0, and at 0 than at -32
 *   - 0 is the sound as made: nudged and back to 0, the same render sample for sample (renders against the code before
 *     these offsets existed were checked the same, 211 of them, when it was written: CHANGELOG)
 *   - the engine's own envelope page after the common one still changes the sound, with the offsets set
 *   - the pages: ENV and LFO open on the common page, the engine's own next; greyed cells say "--"; a kit has no LFO
 *     page; a turn of a greyed knob does nothing
 *   - the offsets go with a saved Line-up (and an old record without them loads at 0), back to 0 on a sound load or a
 *     factory Line-up, and back with Undo
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -DMELODEE_USB_AUDIO=1 -Itests \
 *      -o build/host/dock_front_test tests/dock_front_test.c -lm */
#include "../emu/dock_web.c"
#include <sys/mman.h>
#include <sys/wait.h>

static int fails;
static void fcheck(const char *what, int ok)
{
    printf("front: %-86s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_ms(uint32_t ms) { uint32_t e = fm1_ms + ms; while ((int32_t)(fm1_ms - e) < 0) web_render(128); }

/* a part's three notes (Keys: a chord), each 0.4 s held and 0.6 s let go: 3 s, mono sum */
#define NS (44100 * 3)
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
/* renders from the state now (forked: each from the same state) with part k's offsets set to o[c][] */
static float *shm;
static void render_at(uint32_t k, int slot, const int16_t *o)
{
    pid_t p = fork();
    if (!p) {
        memcpy(dock_front[k], o, sizeof dock_front[k]);
        play(k, shm + (size_t)slot * NS);
        _exit(0);
    }
    waitpid(p, 0, 0);
}
static void renders(uint32_t k, int nc, const int16_t (*o)[FR_N])
{
    for (int c = 0; c < nc; c++)
        render_at(k, c, o[c]);
}
static double diff_db(int a, int b)                      /* the difference against render a, dB (-200: none) */
{
    const float *x = shm + (size_t)a * NS, *y = shm + (size_t)b * NS;
    double e = 0, d = 0;
    for (int i = 0; i < NS; i++) { e += (double)x[i] * x[i]; d += (double)(x[i] - y[i]) * (x[i] - y[i]); }
    return d > 0 ? 10 * log10(d / (e + 1e-20)) : -200;
}
static int same(int a, int b) { return !memcmp(shm + (size_t)a * NS, shm + (size_t)b * NS, NS * sizeof(float)); }
/* how long a held note's release (rel) or its decay from the peak lasts until 30 dB down, ms, with offset k at off */
static double t30(uint32_t part, uint32_t k, int off, int rel)
{
    double pk = 0, *fr = malloc(600 * sizeof *fr);
    int tp = 0, f, r = -1;
    uint32_t note = part == DOCK_DRUMS ? 36u : 52u;
    int16_t keep = dock_front[part][k];
    dock_front[part][k] = (int16_t)off;
    web_midi(0x90 | part, note, 100);
    for (f = 0; f < 600; f++) {
        double s = 0;
        if (rel && f == 30) web_midi(0x80 | part, note, 0);
        for (int b = 0; b < 4; b++) { web_render(110); for (int i = 0; i < 110; i++) s += (double)out_l[i] * out_l[i]; }
        fr[f] = s;
        if (s > pk && (!rel || f < 30)) { pk = s; tp = f; }
    }
    web_midi(0x80 | part, note, 0);
    for (f = 0; f < 200; f++) {                         /* until the voice has ended (an EP's ringing tine struck */
        uint32_t live = 0;                              /* again keeps its decay: a fresh strike each take) */
        for (uint32_t v = 0; v < NVOICE; v++) live |= trk[part].v[v].active;
        if (!live) break;
        run_ms(50);
    }
    run_ms(200);
    dock_front[part][k] = keep;
    for (f = rel ? 30 : tp; f < 600 && r < 0; f++)
        if (fr[f] < pk * 0.001) r = (f - (rel ? 30 : tp)) * 10;
    free(fr);
    return r < 0 ? 6000 : r;
}

typedef struct { const char *name; uint32_t part, engine, preset; } sound_t;
static const sound_t SOUNDS[] = {
    {"PROPHET Feature My Bass", DOCK_BASS, ENGI_PROPHET, 135}, {"PROPHET Faint Memories", DOCK_LEAD, ENGI_PROPHET, 127},
    {"FM6 4", DOCK_KEYS, ENGI_FM6, 4}, {"CZ-1 ELEC.PIANO", DOCK_LEAD, ENGI_CZ, 28}, {"SID 1", DOCK_LEAD, 3, 1},
    {"JUNO STR MACHINE", DOCK_KEYS, ENGI_JUNO, 2}, {"EP SUITCASE", DOCK_KEYS, ENGI_EP, 1},
    {"organ MOD HAMMOND", DOCK_KEYS, ENGI_WHEEL, 5}, {"PLUCK UPRIGHT", DOCK_BASS, ENGI_PHYS, 9},
    {"PLUCK SURF TWANG", DOCK_LEAD, ENGI_PHYS, 10}, {"DRUM 808 KIT", DOCK_DRUMS, ENGI_DRUM, 0},
};
static const char *const FR_NAME[FR_N] = {"ATK", "DEC", "SUS", "REL", "RATE", "DEPTH"};

static void load(const sound_t *s)
{
    undo_depth++;
    engine_load_of(&trk[s->part], s->engine, s->preset);
    undo_depth--;
    for (uint32_t f = 0; f < FX_SLOTS; f++) trk[s->part].p[P_FXSEND + f] = 0;   /* (dry: the room blurs it) */
    song.sel = s->part;
    run_ms(400);
}

/* every control on every engine: live ones move the sound (both ways together), greyed ones do nothing; 0 again is
 * the sound as made */
static void controls(const sound_t *s)
{
    char what[120];
    load(s);
    for (uint32_t k = 0; k < FR_N; k++) {
        int16_t o[3][FR_N] = {{0}};
        int live = dock_front_live(&trk[s->part], k);
        if (k == FR_RATE) o[0][FR_DEPTH] = o[1][FR_DEPTH] = o[2][FR_DEPTH] = 32;   /* (RATE: an LFO going) */
        o[1][k] = k == FR_DEPTH ? -64 : -48;
        o[2][k] = 48;
        renders(s->part, 3, o);
        double d = diff_db(0, 1) > diff_db(0, 2) ? diff_db(0, 1) : diff_db(0, 2);
        if (live) {
            snprintf(what, sizeof what, "%s: %s moves it (%.0f dB)", s->name, FR_NAME[k], d);
            fcheck(what, d > -40.0);
        } else {
            snprintf(what, sizeof what, "%s: %s greyed, does nothing", s->name, FR_NAME[k]);
            fcheck(what, same(0, 1) && same(0, 2));
        }
    }
    {   /* 0 again: every offset but RATE (which moves the LFO's phase on) nudged for a while, then back at 0: the
         * next render as one from the same moment that never left 0 (the engines' caches made again) */
        pid_t p;
        for (int c = 0; c < 2; c++)
            if (!(p = fork())) {
                for (uint32_t k = 0; k < FR_N; k++) dock_front[s->part][k] = c && k != FR_RATE ? 40 : 0;
                run_ms(300);
                memset(dock_front[s->part], 0, sizeof dock_front[s->part]);
                play(s->part, shm + (size_t)c * NS);
                _exit(0);
            } else
                waitpid(p, 0, 0);
        snprintf(what, sizeof what, "%s: at 0, nudged and back, the sound as made", s->name);
        fcheck(what, same(0, 1));
    }
}

/* "+" longer: REL (released) and DEC (from the peak) at -32, 0, +32 */
static void longer(const sound_t *s, uint32_t k)
{
    char what[120];
    double a, b, c;
    load(s);
    a = t30(s->part, k, -32, k == FR_REL);
    b = t30(s->part, k, 0, k == FR_REL);
    c = t30(s->part, k, 32, k == FR_REL);
    snprintf(what, sizeof what, "%s: %s -32 / 0 / +32: %.0f < %.0f < %.0f ms", s->name, FR_NAME[k], a, b, c);
    fcheck(what, a < b && b < c);
}

/* the engine's own envelope page (the next one ENV visits) still changes the sound, the offsets set */
static uint32_t page_of(const char *title)
{
    for (uint32_t i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, title)) return i;
    return 0;
}
static void own_page(const sound_t *s)
{
    uint8_t pg[NPAGES];
    char what[120];
    uint32_t n, c, turned = 0;
    int16_t o[2][FR_N] = {{10, 10, -10, 20, 0, 0}, {10, 10, -10, 20, 0, 0}};
    load(s);
    track_select(s->part);
    go_home();
    open_family(FAM_ENV);
    n = fam_pages(FAM_ENV, pg);
    snprintf(what, sizeof what, "%s: ENV opens on the common page, %u pages", s->name, n);
    fcheck(what, n >= 2u && ui.page == pg[0] && PAGES[pg[0]].scope == SC_FRONT);
    open_family(FAM_ENV);                               /* ENV again: the next, the engine's own (or the ADSR) */
    render_at(s->part, 0, o[0]);
    for (c = 0; c < 4u && !turned; c++) {               /* a knob of it that moves something */
        int16_t *vp, before;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (!d || !vp || d->min == d->max) continue;
        before = *vp;
        edit_param(c, *vp < d->max ? 12 : -12);
        page_desc(cur_page(), c, &vp);
        turned = *vp != before;
    }
    run_ms(1500);
    render_at(s->part, 1, o[1]);
    snprintf(what, sizeof what, "%s: its own page %s still changes it (%.0f dB, offsets set)", s->name,
             cur_page()->title, diff_db(0, 1));
    fcheck(what, turned && diff_db(0, 1) > -40.0);
}

/* the pages: ENV and LFO open on the common one, the engine's own next, the part's after, MOD last; greyed cells */
static uint32_t fam_titles(uint32_t fam, char *b, uint32_t n)   /* "LFO, P5 LFO, .." */
{
    uint8_t pg[NPAGES];
    uint32_t m = fam_pages(fam, pg), i;
    b[0] = 0;
    for (i = 0; i < m; i++) {
        strncat(b, PAGES[pg[i]].title, n - strlen(b) - 1);
        if (i + 1 < m) strncat(b, ", ", n - strlen(b) - 1);
    }
    return m;
}
static void pages(void)
{
    char b[200], what[260];
    static const struct { const sound_t *s; const char *env, *lfo; } P[] = {
        {&SOUNDS[0], "ENV, P5 FLT ENV, P5 AMP ENV, P5 ENV MOD", "LFO, P5 LFO, P5 WHEEL, P5 WHEEL 2, LFO WAVE, LFO SYNC, LFO DEST, MOD"},
        {&SOUNDS[2], "ENV, EG RATE, EG LVL, PITCH EG, PITCH LV", "LFO, FM LFO, LFO WAVE, LFO SYNC, LFO DEST, MOD"},
        {&SOUNDS[5], "ENV, ADSR, ENV DEST", "LFO, LFO WAVE, LFO SYNC, LFO DEST, MOD"},
        {&SOUNDS[6], "ENV, EDIT 2", "LFO, LFO WAVE, LFO SYNC, LFO DEST, MOD"},
        {&SOUNDS[10], "ENV", "LFO WAVE, LFO SYNC, LFO DEST, MOD"},
    };
    for (uint32_t i = 0; i < NELEM(P); i++) {
        load(P[i].s);
        track_select(P[i].s->part);
        fam_titles(FAM_ENV, b, sizeof b);
        snprintf(what, sizeof what, "%s: ENV visits %s", P[i].s->name, b);
        fcheck(what, !strcmp(b, P[i].env));
        fam_titles(FAM_LFO, b, sizeof b);
        snprintf(what, sizeof what, "%s: LFO visits %s", P[i].s->name, b);
        fcheck(what, !strcmp(b, P[i].lfo));
    }
    {   /* EP: ATK and SUS "--", greyed; a turn there does nothing; DEC turns */
        int16_t *vp;
        load(&SOUNDS[6]);
        track_select(DOCK_KEYS);
        go_home();
        open_family(FAM_ENV);
        ui.force = 1;
        ui_draw();
        fcheck("EP: the common ENV's ATK and SUS read \"--\", DEC and REL 0",
               !strncmp(ui.col[0], "ATK|--|", 7) && !strncmp(ui.col[2], "SUS|--|", 7) && !strncmp(ui.col[1], "DEC|0|", 6));
        edit_param(0, 5);
        edit_param(2, 5);
        edit_param(1, 5);
        page_desc(cur_page(), 1, &vp);
        fcheck("EP: turning ATK, SUS does nothing; DEC turns", !dock_front[DOCK_KEYS][FR_ATK] &&
               !dock_front[DOCK_KEYS][FR_SUS] && dock_front[DOCK_KEYS][FR_DEC] > 0 && vp == &dock_front[DOCK_KEYS][FR_DEC]);
        memset(dock_front[DOCK_KEYS], 0, sizeof dock_front[0]);
    }
}

/* kept with a saved Line-up; 0 on a factory one and on a sound load; back with Undo; an old record loads at 0 */
static void saving(void)
{
    static dock_lineup_t r, z;
    dock_lineup_load(0);
    run_ms(200);
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < FR_N; j++) dock_front[k][j] = (int16_t)((int)(k * 7 + j * 3) - 9);
    dock_lineup_capture(&r, 0);
    int ok = 1;
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < FR_N; j++) ok &= r.front[k][j] == (int8_t)((int)(k * 7 + j * 3) - 9);
    fcheck("a Line-up kept: every part's offsets in its record", ok);
    dock_lineup_apply(&DOCK_LINEUPS[0]);
    ok = 1;
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < FR_N; j++) ok &= dock_front[k][j] == 0;
    fcheck("a factory Line-up: all at 0", ok);
    dock_lineup_apply(&r);
    ok = 1;
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t j = 0; j < FR_N; j++) ok &= dock_front[k][j] == r.front[k][j];
    fcheck("the kept one again: its offsets back", ok);
    undo_clear();
    track_select(DOCK_KEYS);
    apply_preset_to(&trk[DOCK_KEYS], 2);
    fcheck("a sound load: that part's at 0 (the others stay)", !dock_front[DOCK_KEYS][FR_DEC] &&
           !dock_front[DOCK_KEYS][FR_DEPTH] && dock_front[DOCK_LEAD][FR_DEC] == r.front[DOCK_LEAD][FR_DEC]);
    undo_step(0);
    ok = 1;
    for (uint32_t j = 0; j < FR_N; j++) ok &= dock_front[DOCK_KEYS][j] == r.front[DOCK_KEYS][j];
    fcheck("Undo: the sound and its offsets back", ok);
    {   /* your Line-ups of before the offsets (a record DOCK_LINEUP_V1 bytes long): load, offsets at 0 */
        dock_store_hdr_t *h = dock_store_hdr();
        (void)proj_wire_borrow();
        memset(proj_wire_u.raw, 0, DOCK_STORE_BYTES(2));
        h->magic = DOCK_STORE_MAGIC;
        h->p_count = P_COUNT;
        h->rec = (uint16_t)DOCK_LINEUP_V1;
        h->n = 2;
        memcpy(proj_wire_u.raw + sizeof *h, &r, DOCK_LINEUP_V1);
        z = DOCK_LINEUPS[1];
        memcpy(proj_wire_u.raw + sizeof *h + DOCK_LINEUP_V1, &z, DOCK_LINEUP_V1);
        st_save(OBJ_BANK0 + DOCK_LU_SLOT, proj_wire_u.raw, (uint32_t)sizeof *h + 2u * DOCK_LINEUP_V1);
        const dock_lineup_t *a = dock_store_get(DOCK_LINEUPS[0].name), *b2;
        ok = a && !memcmp(a, &r, DOCK_LINEUP_V1);
        for (uint32_t k = 0; a && k < NPART; k++)
            for (uint32_t j = 0; j < 8u; j++) ok &= a->front[k][j] == 0;
        b2 = dock_store_get(DOCK_LINEUPS[1].name);
        ok &= b2 && !memcmp(b2, &z, DOCK_LINEUP_V1);
        fcheck("your Line-ups saved before the offsets: both load, every value, offsets 0", ok);
        dock_store_drop(DOCK_LINEUPS[0].name);
        dock_store_drop(DOCK_LINEUPS[1].name);
    }
}

int main(void)
{
    shm = mmap(0, (size_t)NS * 4 * 4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    web_boot();
    run_ms(300);
    for (uint32_t i = 0; i < NELEM(SOUNDS); i++)
        controls(&SOUNDS[i]);
    longer(&SOUNDS[0], FR_REL);                         /* PROPHET */
    longer(&SOUNDS[2], FR_REL);                         /* FM6 */
    longer(&SOUNDS[3], FR_REL);                         /* CZ-1 */
    longer(&SOUNDS[3], FR_DEC);
    longer(&SOUNDS[5], FR_REL);                         /* JUNO */
    longer(&SOUNDS[6], FR_REL);                         /* EP */
    longer(&SOUNDS[6], FR_DEC);
    longer(&SOUNDS[10], FR_DEC);                        /* DRUM */
    for (uint32_t i = 0; i < NELEM(SOUNDS); i++)
        if (SOUNDS[i].engine != ENGI_DRUM)
            own_page(&SOUNDS[i]);
    pages();
    saving();
    printf(fails ? "front: %d FAILED\n" : "front: all passed\n", fails);
    return fails != 0;
}
