/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * The design (the device clock driven by the audio the worklet asks for, the web_* exports, the panel by its printed
 * labels, the 240 x 240 screen) follows Felucca's browser build, web/emu/felucca_web.c of hugelton/Felucca
 * (GPL-3.0, Leo Kuroshita, Hügelton Instruments), itself after charlesvestal/fm1-x0x's web/emu/x0x_web.c.
 *
 * Dock (Melodee) in the browser: the firmware's DSP, sequencer and UI on Melodee's own host stubs
 * (tests/ui_test.c, which builds tests/hostsim.c and the ui*.c files the way the UI tests run them), driven like
 * the device's main loop (main.c fm1_main): every millisecond the clock moves on, the audio due by then renders
 * (mix.c mix_block, CTL frames at a time), and every 15 ms one UI frame runs (ui_input, ui_leds, ui_draw), with
 * ui_input in the millisecond between as main.c's wait loop calls it.
 * The flash: a 1 MiB NOR image (emu/dock_flash.c, Felucca's), MELODEE_FLASH 1: the settings, user presets, projects
 * and your Line-ups go into it, and the page keeps its sectors across a reload (panel/worklet.js). A Line-up's export
 * (the page's "Export Line-up"): emu/dock_export.c.
 * Not here yet: USB, TRS MIDI, the LED power-on sweep.
 *   emu/build.sh web  ->  build/emu/dock_web.wasm
 *   DOCK_WEB_SELFTEST: a native main that boots, presses a few things and writes build/emu/screen.ppm */
#define MELODEE_VERSION "DOCK"
#define UI_TEST_NO_MAIN 1
#define MELODEE_FLASH 1
#define UI_TEST_FLASH_HOOKS "../emu/dock_flash.c"
#include "../tests/ui_test.c"
#include "dock_export.c"

/* ui_test.c declares the key matrix empty (its LEDs are read by id); the real one (hal/fm1_input.h), for the LEDs */
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};

/* --------------------------------------------------------- the device --- */
static uint32_t web_booted, web_last_frame, web_draws_seen;
static int32_t web_master_adc = 700, web_knob = 700 * 16;      /* signed, as main.c's knob */

static void web_power_on(void)
{
    ui_power_on();                             /* main.c melodee_init, as the UI tests do it */
    persist_boot();                            /* main.c: the flash (the page's sectors): settings, presets, projects */
    settings_init();                           /* the palette and preferences */
    palette_set(settings.palette);
    fm6_init();
    undo_depth++;
    dock_lineup_load(0);                       /* Yacht again, yours if you saved one (the flash is read now; after
                                                * fm6_init, as main.c melodee_init loads after it) */
    undo_depth--;
    song.master_q12 = 2048;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    web_last_frame = fm1_ms - 15u;
    web_booted = 1;
}

/* main.c's MASTER pot: the same smoothing and curve (signed: turning it down must not wrap) */
static void web_master_poll(void)
{
    uint32_t k10;
    web_knob += (web_master_adc * 16 - web_knob) / 8;
    k10 = (uint32_t)(web_knob / 16);
    song.master_q12 = (k10 * k10) >> 8;
}

/* one pass of main.c's loop: what the browser has of it */
static void web_frame(void)
{
    web_master_poll();
    ui_input();
    glo_poll();
    settings_poll();
    ui_leds();
    ui_draw();
}

/* output ring, stereo float, filled by web_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

/* one millisecond of the device: the clock, the audio due by now (as audio.c: Q15 out, the scope fed), the loop */
static void web_ms(void)
{
    static int32_t o[2u * CTL];
    fm1_ms++;
    host_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += FS;
    while (web_samples_due >= 1000u * CTL) {
        uint32_t k;
        web_samples_due -= 1000u * CTL;
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            if (k & 1u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)o[2u * k];
            ring[i] = (float)o[2u * k] * (1.0f / 65536.0f);        /* as audio.c: Q15 << OUT_SHIFT (7) into 24 bit, */
            ring[i + 1] = (float)o[2u * k + 1u] * (1.0f / 65536.0f);   /* a -6 dBFS ceiling: the device's level */
        }
    }
    if (!web_booted)
        return;
    if (fm1_ms - web_last_frame >= 15u) {
        web_last_frame = fm1_ms;
        web_frame();
    } else {
        ui_input();
    }
}

#define EXPORT __attribute__((used, visibility("default")))
#undef __attribute__

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}
EXPORT void web_boot(void)
{
    if (!web_booted)
        web_power_on();
}

/* the flash: erased, then the page's saved sectors (web_nor), then boot */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }

/* "Export Line-up": the band as it plays now, as the Line-up playing, as text (emu/dock_export.c); its length, the
 * text at web_lineup_text(); web_lineup_name() its file's name ("yacht.dock") */
static char web_lineup_buf[24576];
static dock_lineup_t web_lineup_rec;
EXPORT char *web_lineup_text(void) { return web_lineup_buf; }
EXPORT uint32_t web_lineup_export(void)
{
    static char names[NPART][24];
    char *sound[NPART];
    uint32_t k;
    momentary_restore();
    dock_lineup_capture(&web_lineup_rec, dock_lineup);
    for (k = 0; k < NPART; k++) {
        int pending;
        uint8_t sel = song.sel;
        song.sel = (uint8_t)(k == sel ? (k + 1u) % NPART : sel);   /* (the loaded sound, not one being browsed) */
        dk_name(k, names[k], sizeof names[k], &pending);
        song.sel = sel;
        sound[k] = names[k];
    }
    return dock_lineup_json(&web_lineup_rec, sound, web_lineup_buf, sizeof web_lineup_buf);
}
EXPORT uint32_t web_lineup_mine(void) { return (dock_mine >> dock_lineup) & 1u; }   /* the one playing is yours */
EXPORT uint32_t web_lineup(void) { return dock_lineup; }
EXPORT char *web_lineup_name(void)
{
    static char f[DOCK_NAME + 6u];
    uint32_t i, n = 0;
    for (i = 0; i < DOCK_NAME && DOCK_LINEUPS[dock_lineup].name[i]; i++) {
        char c = DOCK_LINEUPS[dock_lineup].name[i];
        f[n++] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c == ' ' ? '-' : c;
    }
    str_cpy(f + n, ".dock", 6);
    return f;
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 */
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    host_pressed |= m & ~old;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    host_notes |= mask & ~old;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        host_enc[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023 : (int32_t)v; }
/* a channel message from Web MIDI, as a USB-MIDI packet arrives (usb.c midi_enqueue): 0 = the ring was full */
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 1;
    return (uint32_t)midi_enqueue((status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}

/* the LEDs (ui_input.c ui_leds): lit = fm1_led; dim = the dim planes 0 (keys' layout) and 1 (idle buttons) not lit;
 * breath = plane 2. Bit b = the button labelled b, keys bit k */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}
static const uint8_t *led_dim_plane(void)
{
    static uint8_t d[FM1_NCOL];
    uint32_t c;
    for (c = 0; c < FM1_NCOL; c++)
        d[c] = (uint8_t)((fm1_led_dim[0][c] | fm1_led_dim[1][c]) & ~fm1_led[c]);
    return d;
}
EXPORT uint32_t web_lit_buttons(void) { return led_buttons(fm1_led); }
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(led_dim_plane()); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(led_dim_plane()); }
EXPORT uint32_t web_dim_level(void) { return 4; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_dim[2]); }
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_dim[2]); }
EXPORT uint32_t web_mid_keys(void) { return 0; }
EXPORT uint8_t *web_anim_levels(void) { return 0; }

/* the screen: RGB565 as the panel gets it (high byte first); a change count so the page sends only new frames */
EXPORT uint16_t *web_screen(void) { return host_screen; }
EXPORT uint32_t web_screen_draws(void)
{
    static uint32_t sum_last;
    uint32_t i, s = 0;
    for (i = 0; i < 240u * 240u; i += 7u)           /* cheap: a sample of the pixels */
        s = s * 31u + host_screen[i];
    if (s != sum_last) {
        sum_last = s;
        web_draws_seen++;
    }
    return web_draws_seen;
}
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return song.playing; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
EXPORT uint32_t web_voices(uint32_t part)
{
    uint32_t i, n = 0;
    if (part >= NPART)
        return 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[part].v[i].active;
    return n;
}

#ifdef DOCK_WEB_SELFTEST
static void run_ms(uint32_t ms)                   /* as the worklet drives it: by asking for audio */
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0)
        web_render(128);
}
static void tap_button(uint32_t label)
{
    web_buttons(1u << label);
    run_ms(60);
    web_buttons(0);
    run_ms(60);
}
static void save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    if (!f)
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t p = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((p >> 11) << 3), (uint8_t)(((p >> 5) & 63u) << 2), (uint8_t)((p & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
int main(int argc, char **argv)
{
    float peak = 0;
    uint32_t k;
    web_boot();
    run_ms(500);
    save_ppm(argc > 1 ? argv[1] : "build/emu/screen_home.ppm");
    web_keys(1u << 7);                              /* a key down: C4 on the selected part */
    for (k = 0; k < 40u; k++) {
        web_render(128);
        for (uint32_t i = 0; i < 128u; i++)
            peak = fabsf(out_l[i]) > peak ? fabsf(out_l[i]) : peak;
    }
    printf("selftest: key held, part 1 voices %u, peak %.3f, lit keys %08x\n", web_voices(0), peak, web_lit_keys());
    web_midi(0x91, 60, 100); web_midi(0x91, 64, 100); web_midi(0x91, 67, 100); web_midi(0x91, 71, 100);
    web_midi(0x92, 72, 110);
    run_ms(150);
    save_ppm("build/emu/screen_play.ppm");          /* HOME while the band plays: meters, voices */
    web_keys(0);
    web_midi(0x81, 60, 0); web_midi(0x81, 64, 0); web_midi(0x81, 67, 0); web_midi(0x81, 71, 0); web_midi(0x82, 72, 0);
    run_ms(300);
    tap_button(B_EDIT);
    run_ms(300);
    web_enc(EN_K1 + 1u, 6);                         /* KNOB 2 turning: its cell fills with the member's colour */
    run_ms(60);
    save_ppm(argc > 2 ? argv[2] : "build/emu/screen_edit.ppm");
    {   /* PRESETS on HOME: the browse strip (the engine, the place) for a while, then the knobs again */
        int shown, gone;
        tap_button(B_HOME);
        run_ms(200);
        web_enc(EN_PRESET, 1);
        run_ms(150);
        shown = dk.strip != 0;
        run_ms(DK_BROWSE_MS + 500u);
        gone = dk.strip == 0;
        printf("selftest: PRESETS shows the engine strip (%s), then the knobs (%s)\n", shown ? "ok" : "MISSING",
               gone ? "ok" : "STUCK");
        if (!shown || !gone)
            return 1;
    }
    {   /* Program Change: a member's channel picks entry bank * 128 + program of its list; past the end, nothing */
        uint32_t src, kk, total, ok = 1;
        uint8_t sel = song.sel;
        song.sel = DOCK_LEAD;
        list_scan(LM_ALL, 0xFFFFFFFFu, 0, 140u, &src, &kk, &total);   /* (the Lead's entry 140: bank 1, program 12) */
        song.sel = sel;
        web_midi(0xC2, 0, 0);                           /* ch 3, program 0: the Lead's first sound */
        run_ms(100);
        ok &= trk[DOCK_LEAD].eng_req == ENGI_PROPHET && trk[DOCK_LEAD].preset == 0u;
        web_midi(0xB2, 0, 1);                           /* bank 1 */
        web_midi(0xC2, 12, 0);                          /* program 12: entry 140 */
        run_ms(100);
        ok &= trk[DOCK_LEAD].eng_req == src_engine(src, kk) && (src >= NENGINES || trk[DOCK_LEAD].preset == kk);
        web_midi(0xC3, 1, 0);                           /* ch 4, the drummer, program 1: its second kit */
        run_ms(100);
        ok &= trk[DOCK_DRUMS].eng_req == ENGI_DRUM && trk[DOCK_DRUMS].preset == 1u;
        web_midi(0xB3, 0, 5);                           /* far past the drummer's four kits: nothing changes */
        web_midi(0xC3, 9, 0);
        run_ms(100);
        ok &= trk[DOCK_DRUMS].preset == 1u;
        printf("selftest: Program Change and Bank Select pick each member's sound (%s)\n", ok ? "ok" : "FAILED");
        if (!ok)
            return 1;
    }
    {   /* MASTER all the way down and up: the volume stays within the pot's range (no wrap) */
        int32_t qmax = 0;
        for (k = 0; k <= 1023u; k += 31u) {
            web_master(1023u - k);
            run_ms(16);
            qmax = song.master_q12 > qmax ? song.master_q12 : qmax;
        }
        printf("selftest: MASTER swept down, largest volume %d (%s)\n", qmax, qmax <= 4096 ? "ok" : "WRAPPED");
        if (qmax > 4096)
            return 1;
    }
    printf("selftest: %u screen changes, %u ms\n", web_screen_draws(), fm1_ms);
    return 0;
}
#endif
