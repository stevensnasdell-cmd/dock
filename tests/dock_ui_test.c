/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's band on the real UI (DOCK.md §3, §10 step 2), on Melodee's UI test stubs (tests/ui_test.c): always four parts,
 * Bass and Lead mono legato whatever sound loads, no patterns or recording behind SEQ and REC, no Add / Remove track;
 * the simplified panel (docs/design/DOCK_SIMPLIFY.md): what is gone, what moved, the Band section, EDIT's sections.
 *   cc -O1 -w -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -o build/host/dock_ui_test tests/dock_ui_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
static void dcheck(const char *what, int ok)
{
    printf("dock ui: %-66s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

/* ---- the simplification (docs/design/DOCK_SIMPLIFY.md): what is gone from the panel, what moved where ---- */
static const char *const GONE[] = {"SCALES", "SCL", "CHORD", "MPC", "PROJECT", "STORE", "P5 STORE", "NOTES", "PATTERN",
                                   "TEMPO", "PATTERNS", "SONG", "MOTION", "DRUM HIT"};
static uint8_t seen[NPAGES];
static void see(void)
{
    if (!ui.home && !ui.menu && !smap.on && ui.page < NPAGES)
        seen[ui.page] = 1;
}
static void walk(uint32_t b)                    /* a button tapped again and again from HOME, SELECT on each page; EDIT's
                                                 * section list: every section opened (OCT+), its pages, back (OCT-) */
{
    uint32_t n, k, r;
    go_home();
    frame();
    for (n = 0; n < 6u; n++) {
        press(b);
        see();
        if (smap.on) {
            turn(EN_K2, -20);
            for (r = 0; r < SEC_MAX && smap.on; r++) {
                press(B_OCTUP);
                see();
                for (k = 0; k < 6u; k++) {
                    turn(EN_SELECT, 1);
                    see();
                }
                press(B_OCTDN);
                turn(EN_K2, 1);
            }
            if (smap.on)
                press(B_OCTDN);
            continue;
        }
        for (k = 0; k < 6u; k++) {
            turn(EN_SELECT, 1);
            see();
        }
    }
    go_home();
    frame();
}
static int rows_have(const sheet_row_t *rw, uint32_t n, const char *label)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (str_eq(rw[i].label, label))
            return 1;
    return 0;
}
static int menu_shows(uint32_t id)
{
    uint32_t r;
    for (r = 0; r < MI_SHOWN; r++)
        if (MI_ROWS[r] == id)
            return 1;
    return 0;
}
static void menu_to(uint32_t id)                /* hold HOME, KNOB 2 down to that row */
{
    uint32_t n;
    go_home();
    frame();
    if (!ui.menu)
        hold(B_HOME);
    turn(EN_K2, -40);
    for (n = 0; n < 40u && ui.menu_sel != id; n++)
        turn(EN_K2, 1);
}
static void dock_simplify(void)
{
    static const uint32_t BTN[] = {B_EDIT, B_ENV, B_LFO, B_FX, B_ARP, B_SEQ, B_SCL, B_GLO, B_SAVE};
    uint32_t k, e, b, i, gone = 0, engines = 0, fm6_scale = 0;
    char bad[64] = "";
    ui_power_on();
    frame();
    memset(seen, 0, sizeof seen);
    for (k = 0; k < NPART; k++) {              /* every member, every engine it plays, every page button */
        track_select(k);
        for (e = 0; e < NENGINES; e++) {
            if (!eng_ok(e) || !DOCK_ENGINE_OK(k, e))
                continue;
            select_engine(e);
            frame();
            engines++;
            for (b = 0; b < NELEM(BTN); b++)
                walk(BTN[b]);
        }
    }
    for (i = 0; i < NPAGES; i++)
        for (k = 0; k < NELEM(GONE); k++)
            if (seen[i] && str_eq(PAGES[i].title, GONE[k])) {
                gone++;
                str_cpy(bad + str_len(bad), GONE[k], sizeof bad - str_len(bad));
                str_cpy(bad + str_len(bad), " ", sizeof bad - str_len(bad));
            }
    if (gone)
        printf("dock ui:   (reached: %s)\n", bad);
    for (i = 0, k = 0; i < NPAGES; i++)
        k += seen[i];
    printf("dock ui:   (the walk: %u engines on the four members, %u pages reached)\n", engines, k);
    dcheck("no SCL pages, PROJECT, STORE or sequencer pages anywhere", !gone && engines > 6u);
    fm6_scale = seen[page_titled("SCALE")];
    dcheck("  FM6's keyboard SCALE stays (a sound setting, not a tuning)", fm6_scale);
    dcheck("  CLOCK, SHARED, the member's FX rack, PRESETS, USER reached", seen[page_titled("CLOCK")] &&
           seen[page_titled("SHARED")] && seen[page_titled("FX")] && seen[page_titled("PRESETS")] && seen[page_titled("USER")]);

    ui_power_on();
    frame();
    press(B_SCL);
    dcheck("SCL: nothing (no page)", ui.home == 1u && !ui.msg_t);
    hold(B_SCL);
    dcheck("SCL held: no quick layer", ui.home == 1u && ui.layer != LAYER_SCL && !ui.ly);
    layer_seen = 0;
    press(B_GLO);
    dcheck("GLO tapped (its layer never opened): no HOLD GLO QUICK hint", !ui.home && !(ui.msg_t && str_prefix(ui.msg, "HOLD")));

    go_home();
    frame();
    press(B_FX);
    dcheck("FX: the Line-up's room (SHARED) first", !ui.home && str_eq(cur_page()->title, "SHARED"));
    for (k = 0; k < 8u && !str_eq(cur_page()->title, "FX"); k++)
        press(B_FX);
    dcheck("  FX again: on to the member's own rack", str_eq(cur_page()->title, "FX"));
    for (k = 0; k < 8u && !str_eq(cur_page()->title, "SHARED"); k++)
        press(B_FX);
    dcheck("  and round to SHARED", str_eq(cur_page()->title, "SHARED"));
    press(B_OCTDN);
    dcheck("  OCT- on SHARED: HOME", ui.home == 1u);

    {   /* the sheets (OCT+ held): about the member's sound only */
        uint32_t ok = 1, t;
        for (t = 0; t < 2u; t++) {
            go_home();
            frame();
            if (t)
                go_title("EDIT 1");
            frame();
            fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
            host_pressed |= 1u << panel.btn[B_OCTUP];
            for (k = 0; k < 52u; k++)
                frame();
            fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
            frame();
            ok &= pop.on == POP_SHEET && rows_have(pop.rows, pop.n, "Init sound") && rows_have(pop.rows, pop.n, "Save as user\x85") &&
                  rows_have(pop.rows, pop.n, "Undo last load") && !rows_have(pop.rows, pop.n, "Favourite") &&
                  !rows_have(pop.rows, pop.n, "Sound designer") && !rows_have(pop.rows, pop.n, "Save line-up") &&
                  !rows_have(pop.rows, pop.n, "Factory line-up");
            press(B_OCTDN);
        }
        dcheck("HOME's and a page's sheet: Init, Save as user, Undo; no Favourite, designer, line-ups", ok);
    }

    {   /* the browser: ALL, RECENT, the categories; no FAVES (a FAV list left from Melodee: ALL) */
        uint32_t fav = 0;
        go_title("PRESETS");
        frame();
        for (k = 0; k < LM_N + 2u; k++) {
            turn(EN_K1, -1);
            fav |= list_mode() == LM_FAV;
        }
        dcheck("the browser's list: ALL first", list_mode() == LM_ALL);
        for (k = 0; k < LM_N + 2u; k++) {
            turn(EN_K1, 1);
            fav |= list_mode() == LM_FAV;
        }
        dcheck("  never FAVES, through RECENT and the categories", !fav);
        favorites.filter = 1;
        dcheck("  (Melodee's FAV list kept in the settings: ALL)", list_mode() != LM_FAV);
        favorites.filter = 0;
        list_set(LM_ALL);
        go_home();
        frame();
    }

    {   /* HOME's menu: trimmed, and its Band section */
        static const uint32_t HIDDEN[] = {MI_CLICK, MI_CLICK_LEVEL, MI_COUNTIN, MI_PREVIEW, MI_ADD, MI_AUTOSAVE, MI_HOME_SEL,
                                          MI_GLO_SEL, MI_DESIGNER};
        static const uint32_t KEPT[] = {MI_COLOR, MI_FONT, MI_HOLD, MI_LIGHTS, MI_USB, MI_USB_INPUT, MI_PICKER, MI_ACCEL,
                                        MI_SCREEN, MI_PANEL, MI_ABOUT, MI_LU_SAVE, MI_LU_FACTORY, MI_SPLIT, MI_CHANNELS};
        uint32_t hid = 0, kept = 1, visited = 0, n;
        int16_t route = song.g[G_ROUTE];
        uint8_t share[3] = {dock_share[0], dock_share[1], dock_share[2]};
        for (k = 0; k < NELEM(HIDDEN); k++)
            hid += menu_shows(HIDDEN[k]);
        for (k = 0; k < NELEM(KEPT); k++)
            kept &= menu_shows(KEPT[k]);
        dcheck("the menu: no click, count-in, preview, chord entry, autosave, HOME / GLO SELECT, designer", !hid);
        dcheck("  the preferences, USB, CALIBRATION, ABOUT kept; the Band rows there", kept);
        menu_to(MI_COLOR);
        for (n = 0; n < 40u; n++) {            /* KNOB 2 down the list: the heading never picked */
            visited |= ui.menu_sel == MI_BAND;
            turn(EN_K2, 1);
        }
        dcheck("  KNOB 2 skips the Band heading", !visited && ui.menu_sel == MI_BACK);
        menu_to(MI_SPLIT);
        turn(EN_K1, 1);
        dcheck("  Band: VOICE SPLIT turns the members' voices (eight between B K L)", ui.menu_sel == MI_SPLIT &&
               (dock_share[0] != share[0] || dock_share[1] != share[1] || dock_share[2] != share[2]) &&
               dock_share[0] + dock_share[1] + dock_share[2] == 8u);
        turn(EN_K1, -1);
        dcheck("  and back", dock_share[0] == share[0] && dock_share[1] == share[1] && dock_share[2] == share[2]);
        menu_to(MI_CHANNELS);
        turn(EN_K1, 1);                        /* (CH1-4, SEL, CH5-8: N_ROUTE's order) */
        turn(EN_K1, 1);
        dcheck("  Band: CHANNELS moves the members' MIDI channels", song.g[G_ROUTE] == 2 && route == 0);
        song.g[G_ROUTE] = route;
        menu_to(MI_LU_FACTORY);
        press(B_OCTUP);
        frame();
        dcheck("  Band: Factory line-up (none of yours): says so, menu closed", !ui.menu && !ui.confirm && ui.msg_t);
        menu_to(MI_LU_SAVE);
        press(B_OCTUP);
        for (n = 0; n < 4u; n++)
            frame();
        dcheck("  Band: Save line-up keeps the band as yours (as hold SAVE)", !ui.menu && ((dock_mine >> dock_lineup) & 1u));
        menu_to(MI_LU_FACTORY);
        press(B_OCTUP);
        dcheck("  Band: Factory line-up (yours playing): its question", ui.confirm == CF_DOCK_FACTORY);
        press(B_OCTUP);
        for (n = 0; n < 4u; n++)
            frame();
        dcheck("  OCT+: the factory one back", !ui.confirm && !((dock_mine >> dock_lineup) & 1u));
        hold(B_SAVE);
        for (n = 0; n < 4u; n++)
            frame();
        dcheck("hold SAVE on HOME still saves the Line-up", (dock_mine >> dock_lineup) & 1u);
        dock_lineup_factory();
        frame();
    }

    {   /* GLOBAL: Dock's voice split in the budget's place; SYSTEM: the member channels as BKLD */
        uint32_t b = page_titled("GLOBAL"), sys = page_titled("SYSTEM");
        char val[16];
        const char *unit;
        uint8_t share0 = dock_share[0];
        dcheck("GLOBAL: A4, the voice SPLIT, CLK, TUNE (no BUDGET)", PAGES[b].id[1] == G_SPLIT && PAGES[b].id[2] == G_CLOCK);
        go_title("GLOBAL");
        turn(EN_K2, 1);
        dcheck("  KNOB 2 turns the split", dock_share[0] != share0 || dock_share[1] != 6u);
        turn(EN_K2, -1);
        param_format(&GP[G_ROUTE], 0, val, &unit);
        dcheck("SYSTEM's member channels: BKLD 1-4 (the card \"1-4\")", PAGES[sys].id[2] == G_ROUTE &&
               str_eq(N_ROUTE[0], "BKLD 1-4") && str_eq(val, "1-4"));
        go_home();
        frame();
    }

    {   /* EDIT: the section list; KNOB 2 a section, OCT+ opens it, SELECT its pages, OCT- back; PRESETS the sounds */
        uint32_t sec0, ok = 1, pr;
        ui_power_on();
        frame();
        track_select(DOCK_KEYS);
        select_engine(ENGI_PROPHET);
        frame();
        go_home();
        frame();
        press(B_EDIT);
        dcheck("EDIT (Prophet): the section list", smap.on && cur_page()->fam == FAM_EDIT);
        press(B_EDIT);
        dcheck("  EDIT again: the list stays", smap.on);
        turn(EN_K2, -20);
        turn(EN_K2, 2);
        press(B_OCTUP);
        sec0 = sec_of(ui.page);
        dcheck("  KNOB 2 a section, OCT+ opens it", !smap.on && sec0 == 2u && cur_page()->fam == FAM_EDIT);
        for (k = 0; k < 8u; k++) {
            turn(EN_SELECT, 1);
            ok &= sec_of(ui.page) == sec0 && cur_page()->fam == FAM_EDIT;
        }
        dcheck("  SELECT turns that section's pages only", ok);
        pr = TSEL->preset;
        turn(EN_PRESET, 1);
        for (k = 0; k < 10u; k++)
            frame();
        dcheck("  PRESETS there: the next sound (no section jump)", TSEL->preset != pr && !smap.on &&
               cur_page()->fam == FAM_EDIT);
        press(B_OCTDN);
        dcheck("  OCT-: back to the section list", smap.on);
        press(B_OCTDN);
        dcheck("  OCT- there: HOME", !smap.on && ui.home == 1u);
        dcheck("HOME: the knobs move the member (PART)", ui.home_pg == 0u && !dock_macro_band);
        {   /* the macros: PART the selected member's, BAND Bass Keys Lead's (Space: the drummer's too), each from its own */
            uint32_t sel = song.sel % NPART, m;
            int ok = 1;
            memset(dock_macro, 0, sizeof dock_macro);
            turn(EN_K1, 5);
            for (m = 0; m < NPART; m++) ok &= dock_macro[m][MC_TONE] == (m == sel ? 5 * MC_DETENT : 0);
            dcheck("  KNOB 1 on HOME (PART): the member's Tone, no one else's", ok);
            press(B_HOME);
            dock_macro[0][MC_SPACE] = 20;
            turn(EN_K2, -3);
            turn(EN_K4, 4);
            for (m = 0, ok = 1; m < NPART; m++)
                ok &= dock_macro[m][MC_BITE] == (m < 3u ? -3 * MC_DETENT : 0) &&
                      dock_macro[m][MC_SPACE] == (m ? 4 * MC_DETENT : 20 + 4 * MC_DETENT);
            dcheck("  BAND: Bite on Bass, Keys, Lead; Space on all four, each from its own value", ok);
            turn(EN_K3, 100);
            for (m = 0, ok = 1; m < 3u; m++) ok &= dock_macro[m][MC_SHAPE] == MC_MAX;
            dcheck("  to the end and no further", ok);
            press(B_HOME);
            press(B_HOME);
            memset(dock_macro, 0, sizeof dock_macro);
        }
        press(B_HOME);
        dcheck("HOME tapped on HOME: BAND", ui.home == 1u && ui.home_pg == 0u && dock_macro_band);
        press(B_HOME);
        dcheck("  again: the chord wheel", ui.home == 1u && ui.home_pg == 1u);
        press(B_HOME);
        dcheck("  again: PART", ui.home == 1u && ui.home_pg == 0u && !dock_macro_band);
    }
}

int main(void)
{
    uint32_t sig;
    ui_power_on();
    frame();

    dcheck("four parts at power-on, whatever the song says", trk_n() == 4u);
    song.ntrk = 1;
    dcheck("  (a song of one track still has four)", trk_n() == 4u);
    dcheck("Bass and Lead mono (every note strikes), Keys poly", trk[DOCK_BASS].p[P_VOICE] == DOCK_MONO_MODE &&
                                                    trk[DOCK_LEAD].p[P_VOICE] == DOCK_MONO_MODE &&
                                                    trk[DOCK_KEYS].p[P_VOICE] == V_POLY);
    apply_preset_to(&trk[DOCK_BASS], 3);          /* PROPHET Forever Keys: a poly preset onto the Bass */
    apply_preset_to(&trk[DOCK_LEAD], 3);
    apply_preset_to(&trk[DOCK_KEYS], 3);
    dcheck("a poly sound loaded on Bass and Lead plays mono", trk[DOCK_BASS].p[P_VOICE] == DOCK_MONO_MODE &&
                                                                      trk[DOCK_LEAD].p[P_VOICE] == DOCK_MONO_MODE);
    dcheck("  and stays poly on Keys", trk[DOCK_KEYS].p[P_VOICE] == V_POLY);
    dcheck("Bass and Lead: last-note priority (a sequencer's slides)", trk[DOCK_BASS].p[P_PRIO] == 0 &&
                                                                         trk[DOCK_LEAD].p[P_PRIO] == 0);

    ui_power_on();
    frame();
    press(B_SEQ);
    dcheck("SEQ: the CLOCK page (BPM, clock source, incoming BPM)", !ui.home && cur_page()->graph == GR_CLOCK &&
           cur_page()->id[0] == G_BPM && cur_page()->id[1] == G_CLOCK && cur_page()->id[2] == G_CLK_IN);
    {
        int16_t bpm = song.g[G_BPM];
        turn(EN_K1, 3);
        dcheck("  KNOB 1 there sets the tempo", song.g[G_BPM] == bpm + 3);
        turn(EN_K3, 5);
        dcheck("  KNOB 3 (IN) shows, never sets", song.g[G_BPM] == bpm + 3);
        song.g[G_BPM] = bpm;
    }
    go_home();
    frame();
    hold(B_SEQ);
    dcheck("SEQ held: no patterns grid, HOME stays", ui.home == 1u && !pattern_keys_on());
    sig = steps_sig(TSEL);
    press(B_REC);
    dcheck("REC: no recording armed, transport stopped", !song.rec && !song.playing);
    hold(B_REC);
    dcheck("REC held: no capture into a pattern", steps_sig(TSEL) == sig && ui.home == 1u);
    dock_simplify();

    {   /* the sound lists: the band's three the same, entry by entry, with no drums; the drummer's only drums */
        uint32_t tot[NPART], h[NPART], drums_in_band = 0, other_in_drums = 0, n, src, kk, e, k, steps_ok = 1;
        for (k = 0; k < NPART; k++) {
            song.sel = (uint8_t)k;
            list_scan(LM_ALL, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &src, &kk, &tot[k]);
            h[k] = 0;
            for (n = 0; n < tot[k]; n++) {
                src = preset_all_at(n, &kk);
                e = src_engine(src, kk);
                h[k] = h[k] * 31u + src * 1009u + kk;
                if (k < DOCK_DRUMS && e == ENGI_DRUM) drums_in_band++;
                if (k == DOCK_DRUMS && e != ENGI_DRUM) other_in_drums++;
            }
            e = trk[k].eng_req;                           /* KNOB 4's engine steps, both ways */
            for (n = 0; n < 2u * NENG_SHOWN; n++) {
                e = eng_step(e, n < NENG_SHOWN ? 1 : -1);
                steps_ok &= DOCK_ENGINE_OK(k, e) != 0;
            }
        }
        song.sel = 0;
        dcheck("Bass, Keys and Lead browse the same list, in the same order", tot[0] == tot[1] && tot[1] == tot[2] &&
                                                                                h[0] == h[1] && h[1] == h[2]);
        dcheck("  with no drum kits in it", drums_in_band == 0u && tot[0] > 0u);
        dcheck("the drummer browses drum kits only", other_in_drums == 0u && tot[DOCK_DRUMS] > 0u);
        dcheck("KNOB 4's engine steps keep to each part's engines", steps_ok);
        printf("dock ui:   (the band's list: %u sounds; the drummer's: %u)\n", tot[0], tot[DOCK_DRUMS]);
    }

    {   /* the Line-ups: Yacht at power-on; SELECT on HOME steps through them, each loading its four sounds */
        uint32_t i, k, same = 1, loads = 1, rules = 1, mono = 1;
        for (k = 0; k < NPART; k++)
            same &= DOCK_LINEUPS[0].part[k].engine == TRK_DEF[k][0] && DOCK_LINEUPS[0].part[k].preset == TRK_DEF[k][1];
        dcheck("Yacht is the power-on line-up (TRK_DEF)", same);
        for (i = 0; i < DOCK_NLINEUPS; i++)
            for (k = 0; k < NPART; k++)
                rules &= DOCK_ENGINE_OK(k, DOCK_LINEUPS[i].part[k].engine) != 0 &&
                         DOCK_LINEUPS[i].part[k].preset < ENGINES[DOCK_LINEUPS[i].part[k].engine]->npresets;
        dcheck("every line-up: drums on the drummer only, real presets", rules);
        ui_power_on();
        frame();
        for (i = 1; i <= DOCK_NLINEUPS; i++) {
            dock_lineup_turn(1);
            frame();
            for (k = 0; k < NPART; k++)
                loads &= trk[k].eng_req == DOCK_LINEUPS[i % DOCK_NLINEUPS].part[k].engine &&
                         trk[k].preset == DOCK_LINEUPS[i % DOCK_NLINEUPS].part[k].preset;
            mono &= trk[DOCK_BASS].p[P_VOICE] == DOCK_MONO_MODE && trk[DOCK_LEAD].p[P_VOICE] == DOCK_MONO_MODE;
        }
        dcheck("SELECT steps through all five, each loading its four sounds", loads && dock_lineup == 0u);
        dock_lineup_turn(-1);
        dcheck("  and back round from Yacht to Goth", dock_lineup == DOCK_NLINEUPS - 1u &&
                                                     trk[DOCK_KEYS].eng_req == DOCK_LINEUPS[DOCK_NLINEUPS - 1u].part[1].engine);
        dcheck("Bass and Lead stay mono in every line-up", mono);
    }

    {   /* Mod and Hot: the organ and PLUCK, by name, as SELECT loads them */
        uint32_t i, mod = 0, hot = 0;
        char nm[NPART][21];
        for (i = 0; i < DOCK_NLINEUPS; i++) {
            if (str_eq(DOCK_LINEUPS[i].name, "Mod")) mod = i;
            if (str_eq(DOCK_LINEUPS[i].name, "Hot")) hot = i;
        }
        ui_power_on();
        frame();
        while (dock_lineup != mod) { dock_lineup_turn(1); frame(); }
        for (i = 0; i < NPART; i++) str_cpy(nm[i], eng_preset(ENGINES[trk[i].eng_req], trk[i].preset)->name, 21);
        dcheck("Mod: Keys WHEEL MOD HAMMOND, rotary on, drive", trk[DOCK_KEYS].eng_req == ENGI_WHEEL &&
               str_eq(nm[DOCK_KEYS], "MOD HAMMOND") && trk[DOCK_KEYS].p[P_E7] > 0 && trk[DOCK_KEYS].p[P_E6] > 0);
        dcheck("Mod: Lead WHEEL VOX COMBO, with vibrato, mono legato", trk[DOCK_LEAD].eng_req == ENGI_WHEEL &&
               str_eq(nm[DOCK_LEAD], "VOX COMBO") && trk[DOCK_LEAD].p[P_LD_PIT] > 0 &&
               trk[DOCK_LEAD].p[P_VOICE] == DOCK_MONO_MODE);
        while (dock_lineup != hot) { dock_lineup_turn(1); frame(); }
        for (i = 0; i < NPART; i++) str_cpy(nm[i], eng_preset(ENGINES[trk[i].eng_req], trk[i].preset)->name, 21);
        dcheck("Hot: Bass PLUCK UPRIGHT, Lead PLUCK SURF TWANG", trk[DOCK_BASS].eng_req == ENGI_PHYS &&
               str_eq(nm[DOCK_BASS], "UPRIGHT") && trk[DOCK_LEAD].eng_req == ENGI_PHYS && str_eq(nm[DOCK_LEAD], "SURF TWANG"));
        dcheck("  the Lead's vibrato gone after Mod (a load resets the LFO)", trk[DOCK_LEAD].p[P_LD_PIT] == 0);
        dcheck("  PHYS shows as PLUCK", str_eq(ENGINES[trk[DOCK_BASS].eng_req]->name, "PLUCK"));
    }

    printf(fails ? "dock ui: %d FAILED\n" : "dock ui: all passed\n", fails);
    return fails != 0;
}
