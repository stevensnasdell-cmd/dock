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
 * Not here yet: the flash (MELODEE_FLASH 0: nothing is kept across a reload), USB, TRS MIDI, the LED power-on sweep.
 *   emu/build.sh web  ->  build/emu/dock_web.wasm
 *   DOCK_WEB_SELFTEST: a native main that boots, presses a few things and writes build/emu/screen.ppm */
#define MELODEE_VERSION "DOCK"
#define UI_TEST_NO_MAIN 1
#include "../tests/ui_test.c"

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
static uint32_t web_master_adc = 700, web_knob = 700u * 16u;

static void web_power_on(void)
{
    ui_power_on();                             /* main.c melodee_init, as the UI tests do it */
    settings_init();                           /* the default palette and preferences (no flash: defaults) */
    palette_set(settings.palette);
    fm6_init();
    song.master_q12 = 2048;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    web_last_frame = fm1_ms - 15u;
    web_booted = 1;
}

/* main.c's MASTER pot: the same smoothing and curve */
static void web_master_poll(void)
{
    uint32_t k10;
    web_knob += (web_master_adc * 16u - web_knob) / 8u;
    k10 = web_knob / 16u;
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
            ring[i] = (float)o[2u * k] * (1.0f / 32768.0f);
            ring[i + 1] = (float)o[2u * k + 1u] * (1.0f / 32768.0f);
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

/* no flash yet: the page's storage calls find an empty, never written one */
static uint8_t web_nor_dummy[4096];
EXPORT uint8_t *web_nor(void) { return web_nor_dummy; }
EXPORT uint32_t web_nor_size(void) { return 0; }
EXPORT void web_nor_erase(void) {}
EXPORT uint32_t web_flash_writes_count(void) { return 0; }

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
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
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
    web_keys(0);
    run_ms(300);
    tap_button(B_EDIT);
    run_ms(300);
    save_ppm(argc > 2 ? argv[2] : "build/emu/screen_edit.ppm");
    printf("selftest: %u screen changes, %u ms\n", web_screen_draws(), fm1_ms);
    return 0;
}
#endif
