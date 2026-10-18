/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Your Line-ups (firmware/src/dock_lineups.c, dock_store.c) on the emulator's firmware build (emu/dock_web.c: the flash
 * on, a NOR image in RAM):
 *   - a factory Line-up loads exactly its record; the factory one exports as its own file (firmware/lineups/)
 *   - hold SAVE on HOME keeps the band (every part's parameters and engine patch, PROPHET, FM6, CZ-1, DRUM; levels,
 *     pans, sends, inserts; the room; the voice split) as yours, marked in the header; SELECT away and back loads it:
 *     every value back, and it renders the same as the record put in place directly (from one state, forked)
 *   - HOME's sheet: Factory line-up asks, then the factory one plays again and yours is gone
 *   - project slot D holds the Line-ups: never loaded, saved or erased as a project
 * Then, with arguments, the parts tests/dock_tests.sh strings together:
 *   reboot NOR BIN   a new power-on from the flash a run left (NOR): yours plays at once, marked, as BIN
 *   import BIN       (built with -DDOCK_RT_H=\"the header\") the file tools/dock_lineup_import.py made from the export is BIN */
#include <unistd.h>
#include <sys/wait.h>
#include "../emu/dock_web.c"
#ifdef DOCK_RT_H
#include DOCK_RT_H
#endif

static int fails;
static void scheck(const char *what, int ok)
{
    printf("save: %-74s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_ms(uint32_t ms) { uint32_t e = fm1_ms + ms; while ((int32_t)(fm1_ms - e) < 0) web_render(128); }
static void push_for(uint32_t label, uint32_t ms) { web_buttons(1u << label); run_ms(ms); web_buttons(0); run_ms(80); }
static int same(const dock_lineup_t *a, const dock_lineup_t *b) { return !memcmp(a, b, sizeof *a); }
static void diff(const char *what, const dock_lineup_t *a, const dock_lineup_t *b)   /* (a failure's detail) */
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    uint32_t i, n = 0;
    for (i = 0; i < sizeof *a && n < 8u; i++)
        if (x[i] != y[i]) { printf("      %s: byte %u: %u vs %u\n", what, i, x[i], y[i]); n++; }
}
static void shot(const char *path)                     /* the screen as a PPM */
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (uint32_t i = 0; i < 240u * 240u; i++) {
        uint16_t p = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((p >> 11) << 3), (uint8_t)(((p >> 5) & 63u) << 2), (uint8_t)((p & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static int write_file(const char *path, const void *p, size_t n)
{
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(p, 1, n, f) == n;
    if (f) fclose(f);
    return ok;
}
static int read_file(const char *path, void *p, size_t n)
{
    FILE *f = fopen(path, "rb");
    int ok = f && fread(p, 1, n, f) == n;
    if (f) fclose(f);
    return ok;
}

/* the band for 2 s: what a child renders (fork: both children start from the same state), after setup(c) */
static float take[2][44100 * 5];
static uint32_t band(float *o)
{
    static const uint8_t riff[8] = {36, 36, 43, 36, 39, 36, 41, 43}, lead[8] = {72, 74, 75, 79, 77, 75, 74, 72};
    static const uint8_t beat[8] = {36, 42, 38, 42, 36, 36, 38, 42}, chord[2][4] = {{60, 64, 67, 71}, {57, 60, 64, 67}};
    uint32_t n = 0;
    run_ms(300);
    for (uint32_t step = 0; step < 8; step++) {
        if (step % 4u == 0) {
            if (step) for (int i = 0; i < 4; i++) web_midi(0x80 | DOCK_KEYS, chord[0][i], 0);
            for (int i = 0; i < 4; i++) web_midi(0x90 | DOCK_KEYS, chord[step / 4][i], 90);
        }
        if (step) { web_midi(0x80 | DOCK_BASS, riff[step - 1], 0); web_midi(0x80 | DOCK_LEAD, lead[step - 1], 0); }
        web_midi(0x90 | DOCK_BASS, riff[step], 100);
        web_midi(0x90 | DOCK_LEAD, lead[step], 100);
        web_midi(0x90 | DOCK_DRUMS, beat[step], 100);
        for (int b = 0; b < 86; b++) { web_render(128); for (int i = 0; i < 128; i++) { o[n++] = out_l[i]; o[n++] = out_r[i]; } }
    }
    return n;
}
/* renders of the two ways (setup(0), setup(1)) from the state now: the samples that differ (-1: it did not run) */
static long fork_diff(void (*setup)(int), float *peak)
{
    uint32_t n[2] = {0, 0};
    for (int c = 0; c < 2; c++) {
        int fd[2];
        if (pipe(fd)) return -1;
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (!pid) {
            close(fd[0]);
            setup(c);
            uint32_t m = band(take[c]);
            if (write(fd[1], &m, 4) != 4) _exit(1);
            for (uint32_t o = 0; o < m * 4u;) { ssize_t w = write(fd[1], (char *)take[c] + o, m * 4u - o); if (w <= 0) _exit(1); o += (uint32_t)w; }
            _exit(0);
        }
        close(fd[1]);
        if (read(fd[0], &n[c], 4) != 4) n[c] = 0;
        for (uint32_t o = 0; o < n[c] * 4u;) { ssize_t r = read(fd[0], (char *)take[c] + o, n[c] * 4u - o); if (r <= 0) break; o += (uint32_t)r; }
        close(fd[0]);
        waitpid(pid, 0, 0);
    }
    if (!n[0] || n[0] != n[1]) return -1;
    long d = 0;
    *peak = 0;
    for (uint32_t s = 0; s < n[0]; s++) {
        d += take[0][s] != take[1][s];
        *peak = fabsf(take[0][s]) > *peak ? fabsf(take[0][s]) : *peak;
    }
    return d;
}

static dock_lineup_t mine, now, factory_now;
static void put_mine(int c)                             /* 0: yours put in place directly; 1: SELECT loads it */
{
    if (c == 0) { undo_depth++; dock_lineup_apply(&mine); undo_depth--; dock_lineup = 0; }
    else dock_lineup_turn(-1);
}

/* the band edited everywhere a record reaches */
static void edit_band(void)
{
    track_t *b = &trk[DOCK_BASS], *k = &trk[DOCK_KEYS], *l = &trk[DOCK_LEAD], *d = &trk[DOCK_DRUMS];
    uint8_t v[FP_SIZE + 1u];
    b->p[P_LEVEL] = 90; b->p[P_PAN] = -20; b->p[P_FXSEND] = 33; b->p[P_E0] = 17;   /* level, pan, a send, CUT */
    p5_patch_of(b)->raw[P5_CUTOFF] = 40;                 /* the Prophet's own program */
    set_engine_of(k, ENGI_FM6); apply_preset_to(k, 3);  /* the Keys another engine: FM6, its patch edited */
    memcpy(v, fm6_patch[DOCK_KEYS], sizeof v);
    v[0] = (uint8_t)(v[0] > 50 ? 40 : 60);
    fm6_put_patch(DOCK_KEYS, v, 0);
    fm6_fn[DOCK_KEYS][6] = 80;
    k->p[P_ATK] = 20; k->p[P_FXSEND + 1] = 70;
    l->p[P_GLIDE] = 30; l->p[P_FXTYPE] = FX_PHASER; l->p[P_FXTYPE + 1] = 64;   /* CZ-1: an insert on the Lead */
    cz_factory(5, cz_patch[DOCK_LEAD].raw);             /* the CZ-1's own tone: another one (a valid one) */
    d->p[P_LEVEL] = 70; drum_patch[DOCK_DRUMS].c[0][0] = 5; drum_patch[DOCK_DRUMS].c[2][1] = -20;   /* a kit's edits */
    song.g[G_MFXTYPE + 2 * FX_PARAMS] = FX_CHORUS; song.g[G_MFXTYPE + 2 * FX_PARAMS + 1] = 90;     /* the room */
    song.g[G_FXRETURN] = 80; song.g[G_MFXTONE + 2] = 5;
    dock_share[DOCK_BASS] = 2; dock_share[DOCK_KEYS] = 5;   /* the voice split */
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "reboot")) {      /* the flash a run left, then power-on */
        if (!read_file(argv[2], nor, sizeof nor) || !read_file(argv[3], &mine, sizeof mine)) { printf("save: no files\n"); return 1; }
        web_boot();
        run_ms(500);
        dock_lineup_capture(&now, 0);
        scheck("after a reboot: yours plays at power-on, every value as saved", dock_lineup == 0 && same(&now, &mine));
        if (!same(&now, &mine)) diff("reboot", &now, &mine);
        scheck("  and the header marks it", ((dock_mine >> 0) & 1u) && dk.head != 0);
        return fails != 0;
    }
#ifdef DOCK_RT_H
    if (argc >= 3 && !strcmp(argv[1], "import")) {      /* the export, through tools/dock_lineup_import.py */
        if (!read_file(argv[2], &mine, sizeof mine)) { printf("save: no file\n"); return 1; }
        scheck("export -> tools/dock_lineup_import.py -> a factory entry: the same record, byte for byte",
               same(&DOCK_RT[0], &mine));
        if (!same(&DOCK_RT[0], &mine)) diff("import", &DOCK_RT[0], &mine);
        web_boot();
        run_ms(200);
        undo_depth++; dock_lineup_apply(&DOCK_RT[0]); undo_depth--;
        dock_lineup_capture(&now, 0);
        scheck("  and it loads as that state", same(&now, &mine));
        return fails != 0;
    }
#endif
    const char *out = argc > 1 ? argv[1] : "build/host/dock_save";
    char path[256];
    float peak = 0;
    long dd;
    memset(nor, 0xFF, sizeof nor);
    web_boot();
    run_ms(500);

    dock_lineup_capture(&factory_now, 0);
    scheck("power-on: Yacht, its factory record exactly (every value), no mark", dock_lineup == 0 &&
           same(&factory_now, &DOCK_LINEUPS[0]) && !(dock_mine & 1u));
    if (!same(&factory_now, &DOCK_LINEUPS[0])) diff("factory", &factory_now, &DOCK_LINEUPS[0]);
    {   /* every factory Line-up loads as its record */
        int ok = 1;
        for (uint32_t i = 1; i <= DOCK_NLINEUPS; i++) {
            dock_lineup_turn(1); run_ms(30);
            dock_lineup_capture(&now, i % DOCK_NLINEUPS);
            ok &= same(&now, &DOCK_LINEUPS[i % DOCK_NLINEUPS]);
            if (!same(&now, &DOCK_LINEUPS[i % DOCK_NLINEUPS])) diff(DOCK_LINEUPS[i % DOCK_NLINEUPS].name, &now, &DOCK_LINEUPS[i % DOCK_NLINEUPS]);
        }
        scheck("SELECT: every factory Line-up loads exactly its record", ok && dock_lineup == 0);
    }
    {   /* Yacht exported at power-on is firmware/lineups/yacht.dock */
        static char file[24576];
        FILE *f = fopen("firmware/lineups/yacht.dock", "rb");
        size_t n = f ? fread(file, 1, sizeof file - 1, f) : 0;
        if (f) fclose(f);
        file[n] = 0;
        uint32_t len = web_lineup_export();
        scheck("Export at power-on: Yacht's text is firmware/lineups/yacht.dock", n && len == n && !strcmp(file, web_lineup_text()) &&
               str_eq(web_lineup_name(), "yacht.dock"));
    }

    edit_band();
    run_ms(100);
    dock_lineup_capture(&mine, 0);
    push_for(B_SAVE, 1200);                                /* hold SAVE on HOME */
    run_ms(100);
    dock_lineup_capture(&now, 0);
    if (getenv("DBG")) { printf("mine %u msg_t %u '%s' get %p\n", dock_mine, ui.msg_t, ui.msg, (void *)dock_store_get("Yacht"));
        if (dock_store_get("Yacht")) diff("stored", dock_store_get("Yacht"), &mine); diff("now", &now, &mine); }
    scheck("hold SAVE on HOME: kept as yours (the message, the mark)", ((dock_mine >> 0) & 1u) && str_eq(ui.msg, "YACHT SAVED") &&
           dock_store_get("Yacht") && same(dock_store_get("Yacht"), &mine) && same(&now, &mine));
    run_ms(4000);                                       /* (the message gone: HOME's own header, with its dot) */
    {
        uint32_t x, lit = 0, xd = 232 - (uint32_t)text_w(TY_WM, "DOCK") - 8 - DK_MINE_D / 2;
        for (x = xd - 3; x <= xd + 3; x++)
            lit += host_screen[(H_HEAD / 2 + 1) * 240 + x] != host_screen[2 * 240 + x];
        scheck("  the header: a dot after the Line-up's name", lit >= 3 && dk.head != 0);
    }
    shot("build/host/dock_save_home.ppm");

    dock_lineup_turn(1); run_ms(100);
    scheck("SELECT on: Kraut, factory (no mark)", dock_lineup == 1 && !((dock_mine >> 1) & 1u));
    dock_lineup_turn(-1); run_ms(100);
    dock_lineup_capture(&now, 0);
    scheck("SELECT back: yours, every parameter, patch, send, insert, room, voice split", same(&now, &mine) &&
           ((dock_mine >> 0) & 1u) && dock_share[DOCK_BASS] == 2 && trk[DOCK_KEYS].eng_req == ENGI_FM6);
    if (!same(&now, &mine)) diff("load", &now, &mine);
    dock_lineup_turn(1); run_ms(200);                   /* (from Kraut: the record put in place, or SELECT's load) */
    dd = fork_diff(put_mine, &peak);
    printf("      renders: %ld samples differ, peak %.3f\n", dd, peak);
    scheck("  it renders the same as the record put in place directly", dd == 0 && peak > 0.001f);
    dock_lineup_turn(-1); run_ms(100);

    {   /* export yours (for the import round trip) */
        uint32_t len = web_lineup_export();
        snprintf(path, sizeof path, "%s.dock", out);
        scheck("Export: yours as text", len > 1000u && len < sizeof web_lineup_buf - 1u && write_file(path, web_lineup_text(), len));
        snprintf(path, sizeof path, "%s.bin", out);
        write_file(path, &mine, sizeof mine);
        snprintf(path, sizeof path, "%s.nor", out);
        write_file(path, nor, sizeof nor);               /* (the reboot: the flash as now) */
    }

    {   /* project slot D: the Line-ups' flash */
        char nm[16];
        project_load(3);
        int refused = str_eq(ui.msg, "D HOLDS LINE-UPS");
        scheck("project slot D: named LINE-UPS, refuses to load, save, erase", project_name(3, nm) && str_eq(nm, "LINE-UPS") &&
               project_save(3) != 0 && project_erase(3) != 0 && !project_used(3) && project_old(3) && refused &&
               dock_store_get("Yacht") != 0);
        if (getenv("DBG")) { int r = project_save(0); printf("project_save(0) %d '%s'\n", r, ui.msg); }
        scheck("  project A saves and loads as before", project_save(0) == 0 && project_used(0) && dock_store_get("Yacht") != 0);
    }

    {   /* HOME's sheet: Factory line-up, its question, then the factory Yacht */
        dock_lineup_turn(1); run_ms(50); dock_lineup_turn(-1); run_ms(100);   /* (back on yours) */
        push_for(B_OCTUP, 900);                            /* OCT+ held: HOME's sheet */
        int sheet = pop.on == POP_SHEET && pop.rows == SHEET_STAGE;
        for (int i = 0; i < 9; i++) { web_enc(EN_K2, 1); run_ms(30); }   /* KNOB 2: its last row */
        int row = sheet && pop.rows[pop.sel].act == ps_lu_factory;
        push_for(B_OCTUP, 60);                             /* OCT+: the question */
        int asked = ui.confirm == CF_DOCK_FACTORY;
        if (getenv("DBG")) printf("sheet %d (pop %u) row %d sel %u asked %d confirm %u msg '%s'\n", sheet, pop.on, row, pop.sel, asked, ui.confirm, ui.msg);
        shot("build/host/dock_save_factory_ask.ppm");
        push_for(B_OCTUP, 60);                             /* OCT+: yes */
        run_ms(100);
        dock_lineup_capture(&now, 0);
        scheck("HOME's sheet: Factory line-up asks first", sheet && row && asked);
        scheck("  then Yacht is the factory one again: every value, no mark, yours gone", same(&now, &DOCK_LINEUPS[0]) &&
               !(dock_mine & 1u) && !dock_store_get("Yacht") && dock_share[DOCK_BASS] == DOCK_LINEUPS[0].share[0]);
        if (!same(&now, &DOCK_LINEUPS[0])) diff("factory", &now, &DOCK_LINEUPS[0]);
        push_for(B_OCTUP, 900); for (int i = 0; i < 9; i++) { web_enc(EN_K2, 1); run_ms(30); } push_for(B_OCTUP, 60);
        scheck("  asked again with nothing saved: says so, asks nothing", !ui.confirm && !pop.on);
        undo_depth++; dock_lineup_apply(&mine); undo_depth--;   /* yours back on the flash, for the reboot */
        push_for(B_OCTUP, 900); for (int i = 0; i < 9; i++) { web_enc(EN_K2, 1); run_ms(30); }
        web_enc(EN_K2, -1); run_ms(30);                 /* the sheet's Save line-up, one row up */
        int saverow = pop.rows[pop.sel].act == ps_lu_save;
        push_for(B_OCTUP, 60); run_ms(100);
        scheck("  the sheet's Save line-up keeps it again", saverow && dock_store_get("Yacht") && same(dock_store_get("Yacht"), &mine));
        snprintf(path, sizeof path, "%s.nor", out);
        write_file(path, nor, sizeof nor);
    }
    {   /* playing: hold SAVE stops first, then keeps it (as a project's quick save) */
        transport_req = 1; run_ms(100);
        int playing = song.playing;
        push_for(B_SAVE, 1200); run_ms(200);
        scheck("hold SAVE while playing: the transport stops, then it saves", playing && !song.playing &&
               dock_store_get("Yacht") != 0);
    }
    printf(fails ? "save: %d FAILED\n" : "save: all passed\n", fails);
    return fails != 0;
}
