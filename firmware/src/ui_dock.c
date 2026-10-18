/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's HOME (docs/design/DOCK_STYLE.md §6, the reference docs/design/dock/ref/home.png), in place of Melodee's Stage
 * under DOCK_BAND: the header (the DOCK wordmark, the Line-up, the clock, the BPM, PART or BAND), the knob strip always
 * there (the macros: Tone, Bite, Shape, Space, dock_macro.c), a row per part (its badge, its sound, a five-step meter
 * of what it plays) and the band's eight voices coloured by owner. Each zone remembers what it drew
 * and redraws only when that changes; the meters every other frame. Built on Melodee's canvas (gfx.c) and grid (ui.c). */
#include "dock_lineups.c"                               /* the Line-ups: SELECT on HOME (dock_lineup_turn) */
#define DK_ROW_Y 84                                     /* the rows: 84 + 32 k, 30 tall */
#define DK_ROW_H 30
#define DK_ROW_DY 32
#define DK_METER_X 204                                  /* the meter: five bars from here, in a row (x in the row) */
#define DK_VOX_Y 212                                    /* the voices' bar */

#define DK_BROWSE_MS 2000u                              /* the browse strip stays this long after a sound changes */

/* HOME's knobs, the macros (dock_macro.c): KNOB 1-4 Tone, Bite, Shape, Space of the selected member (PART) or of the
 * band (BAND: Bass, Keys and Lead each from its own value; Space the drummer's too). HOME tapped on HOME turns PART ->
 * BAND -> the chord wheel (ui_view.c, the knobs still the band's) -> PART */
static uint8_t dock_macro_band;
static const char *const MC_NAME[MC_N] = {"Tone", "Bite", "Shape", "Space"};
#define MC_DETENT 2                                     /* steps a detent: the middle to an end in 32 */
static void dock_macro_turn(uint32_t k, int32_t s)
{
    uint32_t i;
    k %= MC_N;
    for (i = 0; i < NPART; i++)
        if (dock_macro_band ? i != DOCK_DRUMS || k == MC_SPACE : i == song.sel % NPART)
            dock_macro[i][k] = (int16_t)clamp(dock_macro[i][k] + s * MC_DETENT, MC_MIN, MC_MAX);
}
static void dock_home_tap(void)
{
    if (ui.home_pg) {                                   /* the chord wheel -> PART */
        ui.home_pg = 0;
        dock_macro_band = 0;
    } else if (!dock_macro_band) {                      /* PART -> BAND */
        dock_macro_band = 1;
    } else {                                            /* BAND -> the chord wheel */
        ui.home_pg = 1;
    }
    ui.force = 1;
}
static struct {
    uint32_t head, cell[4], row[NPART], vox, strip;
    uint32_t snd, browse_ms;                            /* the selected member's sound; when it last changed (| 1) */
    uint32_t lineup_ms;                                 /* when SELECT last changed the Line-up (| 1) */
    uint8_t meter[NPART];
} dk;

/* part k's sound as HOME shows it: the one the browser shows while it waits to load it (*pending), else the loaded one,
 * in full (a Prophet program's name too: sound_name keeps 15 letters) */
static void dk_name(uint32_t k, char *nm, uint32_t size, int *pending)
{
    *pending = k == song.sel && browse_pending();
    nm[0] = 0;
    if (*pending) {
        char tag[6];
        uint32_t kk, src = browse_shown(&kk);
        entry_label(src, kk, tag, nm);
        if (!nm[0]) str_cpy(nm, tag, size);
    } else if (trk[k].eng_req == ENGI_PROPHET) {
        p5_short_name(&trk[k], nm, size);
    } else {
        sound_name(&trk[k], nm);
    }
    if (!nm[0]) str_cpy(nm, ENGINES[eng_idx(trk[k].eng_req)]->name, size);
}

static uint16_t dk_idle(uint16_t c) { return ux_mix(T_BG, c, 55); }   /* a silent member (style guide: 55 %) */

static uint32_t dk_sounding(uint32_t k)                 /* part k's voices held or ringing (not fading out) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[k].v[i].active && trk[k].v[i].stage != 4u;
    return n;
}

#define DK_MINE_D 6                                     /* your Line-up's mark: a dot after its name (across) */
/* the header on HOME: Melodee's (draw_head) whenever it has something to say (a message, a layer, recording, a song
 * playing); else DOCK, the Line-up, the clock (filled: an external clock leads; ring: the internal), the BPM. A
 * Line-up of yours (dock_lineups.c: saved, SELECT loads it) has a dot after its name, while it is the one playing */
static void dk_head(void)
{
    char b[8];
    uint32_t sig, ext = midi_clock.mode != 0u, mine = (dock_mine >> dock_lineup) & 1u;
    int32_t x, tag;
    if (ui.msg_t || ui.layer || song.rec || chain.running || ui.bpm_t) {
        dk.head = 0;
        draw_head();
        return;
    }
    fmt_int(b, song.g[G_BPM]);
    sig = str_hash(1u + ext * 2u + (uint32_t)song.playing * 4u + dock_lineup * 8u + mine * 256u + dock_macro_band * 512u, b) +
          ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == dk.head)
        return;
    dk.head = sig;
    ui.head_sig = 0;                                    /* (Melodee's header draws itself whole when it comes back) */
    cv_begin(240, H_HEAD, T_BG);
    cv_disc(12, H_HEAD / 2, 8, T_TEXT, !ext);           /* the clock and the tempo at the left */
    tag = cv_text_on(22, ty_y(H_HEAD / 2, TY_MB), TY_MB, b, T_TEXT, T_BG) + 6;
    /* what the knobs move: PART (the selected member, MID) or BAND (the whole band, TEXT) */
    tag = cv_text_on(tag, ty_y(H_HEAD / 2, TY_LB), TY_LB, dock_macro_band ? "BAND" : "PART", dock_macro_band ? T_TEXT : T_MID,
                     T_BG);
    x = 232 - text_w(TY_WM, "DOCK");                    /* the Line-up's name and DOCK at the right: "Yacht DOCK" */
    cv_text_on(x, ty_y(H_HEAD / 2, TY_WM), TY_WM, "DOCK", T_TEXT, T_BG);
    {
        const char *nm = DOCK_LINEUPS[dock_lineup].name;
        int32_t room = x - 16 - tag - (mine ? DK_MINE_D + 5 : 0);   /* (never over PART / BAND: a size down first) */
        const aafont_t *nf = ty_fit(TY_TX, nm, room < 96 ? room : 96);
        int32_t w = text_w(nf, nm) < 96 ? text_w(nf, nm) : 96;
        if (mine) {                                     /* "Yacht • DOCK": the dot in TEXT (status, not a member) */
            cv_disc(x - 8 - DK_MINE_D / 2, H_HEAD / 2 + 1, DK_MINE_D, T_TEXT, 0);
            x -= DK_MINE_D + 5;
        }
        cv_free_text(x - 8 - w, ty_y(H_HEAD / 2, nf), nf, nm, T_MID, T_BG, 96);
    }
    cv_blit(0, Y_HEAD);
}

/* the knob strip: the macros, four cells 57 x 48, the name and a gauge from its middle (the sound as made: a dot
 * there) to the offset; PART: the selected member's, in TEXT; BAND: Bass, Keys and Lead's, a line each in its colour.
 * The knob turning fills its cell: PART with the member's colour; BAND a raised SURF (a fill would hide the band's
 * coloured lines), the lines a row each, 3 px with 2 px between, each on its own track */
#define DK_GX 4                                         /* the gauge: x, width, its middle */
#define DK_GW (CARD_W - 9)
#define DK_GC (DK_GX + DK_GW / 2)
static void dk_gauge_line(int32_t v, int32_t y, int32_t h, uint16_t c)
{
    int32_t px = v * (DK_GW / 2 - 1) / -MC_MIN;
    cv_rect(px < 0 ? DK_GC - 1 + px : DK_GC - 1, y, (px < 0 ? -px : px) + 2, h, c);
}
static void dk_cells(void)
{
    uint16_t mc = T_TRK(song.sel);
    uint32_t c, k, band = dock_macro_band;
    for (c = 0; c < MC_N; c++) {
        const char *lab = cell_label(MC_NAME[c], CARD_W - 8);   /* (its short form if need be: Melodee Pixel) */
        const aafont_t *lf = ty_fit(TY_LB, lab, CARD_W - 8);
        uint32_t hot = c == ui.hot_col && ui.hot_t, sig;
        uint16_t bg = hot ? band ? ux_mix(T_SURF, T_TEXT, 16) : mc : T_SURF, fg = hot && !band ? T_INK : T_TEXT;
        sig = 1u + hot * 2u + band * 4u + song.sel * 13u + ux.gen * 977u + ux.pal * 31u;
        for (k = 0; k < NPART; k++)
            sig = sig * 131u + (uint32_t)(dock_macro[k][c] + 64);
        if (!ui.force && sig == dk.cell[c])
            continue;
        dk.cell[c] = sig;
        cv_begin(CARD_W, CARD_H, T_BG);
        cv_rrect(0, 0, CARD_W, CARD_H, 8, bg, T_BG);
        cv_free_text(4, ty_y(19, lf), lf, lab, fg, bg, CARD_W - 8);
        if (band) {                                     /* Bass, Keys, Lead: a row each, 3 px, 2 px apart */
            for (k = 0; k < 3u; k++) {
                int32_t y = 29 + 5 * (int32_t)k;
                cv_rect(DK_GX, y, DK_GW, 3, hot ? ux_mix(bg, T_TEXT, 14) : T_LINE);
                dk_gauge_line(dock_macro[k][c], y, 3, T_TRK(k));
            }
        } else {
            cv_rrect(DK_GX, 34, DK_GW, 3, 1, hot ? ux_mix(bg, T_INK, 30) : T_LINE, bg);   /* the gauge's track */
            dk_gauge_line(dock_macro[song.sel % NPART][c], 34, 3, fg);
        }
        cv_blit((uint32_t)CARD_X(c), Y_LABEL);
    }
}

/* the meter of part k: five bars, 3 px wide, 6 apart, rising 4 .. 16 px, lit up to its level; silent: five dots */
static void dk_meter(uint32_t k, uint16_t tc, uint16_t bg, int32_t ox, int32_t oy)
{
    uint32_t i;
    for (i = 0; i < 5u; i++) {
        int32_t x = ox + 6 * (int32_t)i, h = 4 + 3 * (int32_t)i;
        if (dk.meter[k] > i)
            cv_rrect(x, oy + 22 - h, 3, h, 1, tc, bg);
        else
            cv_rrect(x, oy + 19, 3, 3, 1, T_LINE, bg);
    }
}

static uint32_t dk_level(uint32_t k)                    /* part k's peak since the last frame -> 0..5 bars, falling one a frame */
{
    track_t *t = &trk[k];
    int32_t pk = t->peak, m = t->p[P_MUTE] ? 0 : meter_px(pk) * 5 / 24;
    t->peak = 0;
    if (m > 5) m = 5;
    if (m < (int32_t)dk.meter[k] - 1) m = (int32_t)dk.meter[k] - 1;
    return (uint32_t)(m < 0 ? 0 : m);
}

/* a row: its badge (dimmed while silent), the sound (20 px; 16 px if it would not fit), the meter; the picked row tinted */
static void dk_row(uint32_t k)
{
    char nm[24];
    uint32_t sel = k == song.sel, live = dk_sounding(k) != 0u, mute = trk[k].p[P_MUTE] != 0, lv = dk_level(k), sig;
    uint16_t tc = T_TRK(k), bg = sel ? ux_mix(T_SURF, tc, 18) : T_SURF;
    int32_t y = DK_ROW_Y + DK_ROW_DY * (int32_t)k, room = DK_METER_X - 6 - 36;
    char d[2] = {DOCK_BADGE(k), 0};
    const aafont_t *nf;
    int pending;
    dk_name(k, nm, sizeof nm, &pending);
    nf = ty_fit(TY_TX, nm, room);
    sig = str_hash(1u + sel * 2u + live * 4u + mute * 8u + (uint32_t)pending * 16u, nm) + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == dk.row[k]) {
        if (lv != dk.meter[k]) {                        /* the meter alone */
            dk.meter[k] = (uint8_t)lv;
            cv_begin(30, DK_ROW_H, bg);
            dk_meter(k, tc, bg, 0, 0);
            cv_blit((uint32_t)(BOX_X + DK_METER_X), (uint32_t)y);
        }
        return;
    }
    dk.row[k] = sig;
    dk.meter[k] = (uint8_t)lv;
    cv_begin(BOX_W, DK_ROW_H, T_BG);
    cv_rrect(0, 0, BOX_W, DK_ROW_H, 7, bg, T_BG);
    cv_rrect(3, 3, 24, 24, 6, live || sel ? tc : dk_idle(tc), bg);
    cv_text_c(15, ty_y(15, TY_TX), TY_TX, d, T_INK, live || sel ? tc : dk_idle(tc));
    cv_free_text(34, ty_y(DK_ROW_H / 2, nf), nf, nm, pending ? T_ACCENT : mute ? T_DIM : live || sel ? T_TEXT : T_MID, bg,
                 room);
    dk_meter(k, tc, bg, DK_METER_X, 0);
    cv_blit(BOX_X, (uint32_t)y);
}

/* the band's eight voices by owner (voice.c dock_share: Bass, Keys, Lead), lit while sounding; free ones outlined */
static void dk_voices(void)
{
    uint32_t k, i, n = 0, sig = ux.gen * 977u + ux.pal * 31u, on[3];
    for (k = 0; k < 3u; k++) {
        on[k] = dk_sounding(k);
        sig = sig * 31u + on[k];
#if DOCK_VOICES
        sig = sig * 7u + dock_share[k];                 /* (the split changed: the menu's Band, GLOBAL) */
#endif
    }
    if (!ui.force && sig == dk.vox)
        return;
    dk.vox = sig;
    cv_begin(240, 240u - DK_VOX_Y, T_BG);
    cv_text_on(8, ty_y(14, TY_LB), TY_LB, "VOICES", T_MID, T_BG);
    for (k = 0; k < 3u; k++) {
#if DOCK_VOICES
        uint32_t share = dock_share[k];
#else
        uint32_t share = k == 1u ? 6u : 1u;
#endif
        for (i = 0; i < share && n < 8u; i++, n++) {
            int32_t x = 82 + 19 * (int32_t)n;
            if (i < on[k])
                cv_rrect(x, 7, 15, 15, 4, T_TRK(k), T_BG);
            else {
                cv_rrect(x, 7, 15, 15, 4, T_LINE, T_BG);
                cv_rrect(x + 1, 8, 13, 13, 3, T_SURF, T_LINE);
            }
        }
    }
    cv_blit(0, DK_VOX_Y);
}

#if DOCK_VOICES
/* the voice split (voice.c dock_share): the band's eight voices as Bass / Keys / Lead, turned through these on the
 * menu's Band and on GLOBAL (the drummer's lane is its own); a Line-up keeps its split, hold SAVE keeps a new one */
static const uint8_t DOCK_SPLITS[][3] = {{1, 6, 1}, {2, 5, 1}, {1, 5, 2}, {2, 4, 2}, {3, 4, 1}, {1, 4, 3}};
#define DOCK_NSPLITS NELEM(DOCK_SPLITS)
static uint32_t dock_split_at(void)                     /* the split playing, DOCK_NSPLITS if it is none of these */
{
    uint32_t i;
    for (i = 0; i < DOCK_NSPLITS; i++)
        if (DOCK_SPLITS[i][0] == dock_share[0] && DOCK_SPLITS[i][1] == dock_share[1] && DOCK_SPLITS[i][2] == dock_share[2])
            return i;
    return DOCK_NSPLITS;
}
static void dock_split_set(uint32_t i)
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        dock_share[k] = DOCK_SPLITS[i % DOCK_NSPLITS][k];
    ui.force = 1;
}
static void dock_split_turn(int32_t s)                  /* the next / previous split (no wrap; s 0: the next, wrapping); */
{                                                       /* the first if it is none of these */
    uint32_t i = dock_split_at();
    dock_split_set(i == DOCK_NSPLITS ? 0u : !s ? (i + 1u) % DOCK_NSPLITS :
                   (uint32_t)clamp((int32_t)i + (s > 0 ? 1 : -1), 0, (int32_t)DOCK_NSPLITS - 1));
}
static void dock_split_text(char *b, const char *sep)   /* "1/6/1" (sep "/"), Bass first */
{
    uint32_t k;
    b[0] = 0;
    for (k = 0; k < 3u; k++) {
        if (k)
            str_cpy(b + str_len(b), sep, 4);
        fmt_int(b + str_len(b), dock_share[k]);
    }
}
#endif

/* a page's header: the member's badge, the page's name, the member's voices sounding of its share; Melodee's header
 * (draw_head) whenever it has something to say */
static int dk_page_head(void)
{
    char chip[24], v[8], d[2] = {DOCK_BADGE(song.sel % NTRK), 0};
    uint32_t k = song.sel % NTRK, sig;
#if DOCK_VOICES
    uint32_t share = dock_share[k];
#else
    uint32_t share = trk_nvoice(&trk[k]);
#endif
    uint16_t tc = T_TRK(k);
    if (ui.msg_t || ui.layer || song.rec || chain.running || ui.bpm_t || seq_erase_active(TSEL) || head_waiting()) {
        dk.head = 0;
        return 0;
    }
    head_chip_text(chip);
    fmt_int(v, (int32_t)dk_sounding(k));
    str_cpy(v + str_len(v), "/", 2);
    fmt_int(v + str_len(v), (int32_t)share);
    sig = str_hash(str_hash(3u + k * 5u, chip), v) + ux.gen * 977u + ux.pal * 31u + ui.page * 2654435761u;
    if (!ui.force && sig == dk.head)
        return 1;
    dk.head = sig;
    ui.head_sig = 0;
    cv_begin(240, H_HEAD, T_BG);
    cv_rrect(6, 3, 24, 24, 6, tc, T_BG);
    cv_text_c(18, ty_y(15, TY_TX), TY_TX, d, T_INK, tc);
    cv_free_text(38, ty_y(H_HEAD / 2, TY_TX), ty_fit(TY_TX, chip, 236 - 38 - 44), chip, T_TEXT, T_BG, 236 - 38 - 44);
    cv_text_on(232 - text_w(TY_MN, v), ty_y(H_HEAD / 2, TY_MN), TY_MN, v, T_MID, T_BG);
    cv_blit(0, Y_HEAD);
    return 1;
}

/* the browse strip, in the knob strip's place while a sound is chosen (PRESETS, a Program Change, an undo) and
 * DK_BROWSE_MS after: the engine in the member's colour, the category, the place in the list */
static int dk_browsing(void)
{
    const track_t *t = TSEL;
    uint32_t snd = (uint32_t)t->eng_req * 65537u + t->preset * 257u + t->user * 31u + t->user_native * 7u + song.sel * 3u;
    if (dk.snd && snd != dk.snd)
        dk.browse_ms = fm1_ms | 1u;
    dk.snd = snd;
    if (browse_pending())
        dk.browse_ms = fm1_ms | 1u;
    return dk.browse_ms && fm1_ms - (dk.browse_ms & ~1u) < DK_BROWSE_MS;
}

static void dk_browse_strip(void)
{
    char where[16], line[40];
    uint32_t kk, src, total, cur, e, cat, sig, as, ak;
    uint16_t mc = T_TRK(song.sel);
    int32_t x;
    if (browse_pending())
        src = browse_shown(&kk);
    else
        stage_entry(song.sel, &src, &kk);
    e = eng_idx(src_engine(src, kk));
    cat = entry_cat(src, kk) % CAT_N;
    list_only_eng = src_engine(src, kk);                /* the place within its engine's sounds in the list shown */
    cur = list_scan(list_mode(), src, kk, 0xFFFFFFFFu, &as, &ak, &total);
    list_only_eng = 0xFFFFFFFFu;
    fmt_int(where, (int32_t)cur + 1);
    str_cpy(where + str_len(where), " / ", 4);
    fmt_int(where + str_len(where), (int32_t)total);
    str_cpy(line, cat && cat != CAT_OTHER ? CAT_NAME[cat] : "", sizeof line);
    sig = str_hash(str_hash(e * 13u + song.sel, line), where) + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == dk.strip)
        return;
    dk.strip = sig;
    memset(dk.cell, 0, sizeof dk.cell);                 /* (the knobs draw themselves whole when they come back) */
    cv_begin(BOX_W, CARD_H, T_BG);
    cv_rrect(0, 0, BOX_W, CARD_H, 8, T_SURF, T_BG);
    x = cv_text_on(10, ty_y(17, TY_LB), TY_LB, ENGINES[e]->name, mc, T_SURF);
    if (line[0])
        cv_free_text(x + 10, ty_y(17, TY_LB), TY_LB, line, T_MID, T_SURF, BOX_W - x - 20);
    cv_text_on(10, ty_y(35, TY_MN), TY_MN, where, T_MID, T_SURF);
    cv_blit(BOX_X, Y_LABEL);
}

/* SELECT on HOME: the next or previous Line-up, all four sounds at once */
static void dock_lineup_turn(int32_t s)
{
    int32_t n = (int32_t)DOCK_NLINEUPS;
    dock_lineup_load((uint32_t)((((int32_t)dock_lineup + s) % n + n) % n));
    dk.lineup_ms = fm1_ms | 1u;
}

/* the Line-up strip, in the knob strip's place DK_BROWSE_MS after SELECT turns: LINE-UP, its name in the wordmark's
 * face (yours: the dot after it), its place among them */
static void dk_lineup_strip(void)
{
    char nm[12], where[8];
    uint32_t i, sig, mine = (dock_mine >> dock_lineup) & 1u;
    int32_t e;
    for (i = 0; DOCK_LINEUPS[dock_lineup].name[i] && i + 1u < sizeof nm; i++) {
        char c = DOCK_LINEUPS[dock_lineup].name[i];
        nm[i] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;   /* (the wordmark's face: capitals) */
    }
    nm[i] = 0;
    fmt_int(where, (int32_t)dock_lineup + 1);
    str_cpy(where + str_len(where), " / ", 4);
    fmt_int(where + str_len(where), (int32_t)DOCK_NLINEUPS);
    sig = str_hash(7u + mine * 2u, nm) + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == dk.strip)
        return;
    dk.strip = sig;
    memset(dk.cell, 0, sizeof dk.cell);
    cv_begin(BOX_W, CARD_H, T_BG);
    cv_rrect(0, 0, BOX_W, CARD_H, 8, T_SURF, T_BG);
    cv_text_on(10, ty_y(15, TY_LB), TY_LB, "LINE-UP", T_MID, T_SURF);
    e = cv_text_on(10, ty_y(34, TY_WM), TY_WM, nm, T_TEXT, T_SURF);
    if (mine)                                           /* yours: the header's dot */
        cv_disc(e + 6 + DK_MINE_D / 2, 34, DK_MINE_D, T_TEXT, 0);
    cv_text_on(BOX_W - 10 - text_w(TY_MN, where), ty_y(34, TY_MN), TY_MN, where, T_MID, T_SURF);
    cv_blit(BOX_X, Y_LABEL);
}

static void dock_home_draw(void)
{
    uint32_t k;
    if (ui.force) {                                     /* (all drawn again; what the sound was and when it changed kept:
                                                         * a load forces a redraw) */
        uint32_t snd = dk.snd, bms = dk.browse_ms, lms = dk.lineup_ms;
        memset(&dk, 0, sizeof dk);
        dk.snd = snd;
        dk.browse_ms = bms;
        dk.lineup_ms = lms;
    }
    dk_head();
    if (dk.lineup_ms && fm1_ms - (dk.lineup_ms & ~1u) < DK_BROWSE_MS) {
        dk_browsing();                                  /* (the sounds it loaded: not a browse of their own) */
        dk.browse_ms = 0;
        dk_lineup_strip();
    } else if (dk_browsing())
        dk_browse_strip();
    else {
        if (dk.strip)                                   /* (the strip's gone: the gaps between the cells too) */
            lcd_fill(BOX_X, Y_LABEL, BOX_W, CARD_H, T_BG);
        dk.strip = 0;
        dk_cells();
    }
    if (!ui.force && (ui.frame & 1u))
        return;
    for (k = 0; k < NPART; k++)
        dk_row(k);
    dk_voices();
}
