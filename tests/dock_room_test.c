/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's rooms (firmware/src/dock_lineups.c on Melodee's shared effects, fx.c): each Line-up, loaded as SELECT on HOME
 * loads it, recalls its room (the shared slots, return, routing, every part's sends, the inserts off); the whole band
 * through each room renders finite and below clipping; the room is heard (its tail after the band stops); every tail
 * dies away to silence (no stuck loop, no offset); the SPRING rings longer than it is fed. Then the numbers: the shared
 * bus's host time against a PROPHET voice, and the RAM its delay and reverb buffers take, per Line-up.
 * Runs the emulator's firmware build (emu/dock_web.c).
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -DMELODEE_USB_AUDIO=1 -Itests -o build/host/dock_room_test \
 *      tests/dock_room_test.c -lm */
#include "../emu/dock_web.c"

static int fails;
static void rcheck(const char *what, int ok)
{
    printf("room: %-74s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_ms(uint32_t ms) { uint32_t e = fm1_ms + ms; while ((int32_t)(fm1_ms - e) < 0) web_render(128); }

typedef struct { double e; float peak; uint32_t n, bad; } take_t;
static void take(take_t *t, uint32_t ms)
{
    uint32_t e = fm1_ms + ms;
    while ((int32_t)(fm1_ms - e) < 0) {
        web_render(128);
        for (int i = 0; i < 128; i++) {
            float l = out_l[i], r = out_r[i];
            if (!isfinite(l) || !isfinite(r)) { t->bad++; continue; }
            t->e += (double)l * l + (double)r * r; t->n += 2;
            if (fabsf(l) > t->peak) t->peak = fabsf(l);
            if (fabsf(r) > t->peak) t->peak = fabsf(r);
        }
    }
}
static double db(const take_t *t) { return 10.0 * log10(t->e / (t->n ? t->n : 1) + 1e-20); }
static void all_off(void) { for (uint32_t ch = 0; ch < NPART; ch++) for (int nn = 0; nn < 128; nn++) web_midi(0x80 | ch, nn, 0); }

/* the band for 2 s (a bass riff, two chords, a lead line, a beat), then every voice stopped dead (so what follows is the
 * room alone, not a sound's own release): what plays, and the 0.15 .. 0.75 s after (the tail) */
static void band(take_t *play, take_t *tail)
{
    static const uint8_t riff[8] = {36, 36, 43, 36, 39, 36, 41, 43}, lead[8] = {72, 74, 75, 79, 77, 75, 74, 72};
    static const uint8_t beat[8] = {36, 42, 38, 42, 36, 36, 38, 42}, chord[2][4] = {{60, 64, 67, 71}, {57, 60, 64, 67}};
    for (uint32_t step = 0; step < 8; step++) {
        if (step % 4u == 0) {
            if (step) for (int i = 0; i < 4; i++) web_midi(0x80 | DOCK_KEYS, chord[0][i], 0);
            for (int i = 0; i < 4; i++) web_midi(0x90 | DOCK_KEYS, chord[step / 4][i], 90);
        }
        if (step) { web_midi(0x80 | DOCK_BASS, riff[step - 1], 0); web_midi(0x80 | DOCK_LEAD, lead[step - 1], 0); }
        web_midi(0x90 | DOCK_BASS, riff[step], 100);
        web_midi(0x90 | DOCK_LEAD, lead[step], 100);
        web_midi(0x90 | DOCK_DRUMS, beat[step], 100);
        take(play, 250);
    }
    all_off();
    for (uint32_t k = 0; k < NPART; k++) for (uint32_t v = 0; v < NVOICE; v++) trk[k].v[v].active = trk[k].v[v].gate = 0;
    take(play, 50);
    take(tail, 600);
}

/* does part k's state hold Line-up i's room? */
static int recalls(uint32_t i)
{
    const dock_room_t *r = &DOCK_LINEUPS[i].room;
    int ok = song.g[G_FXMODE] == r->mode && song.g[G_FXRETURN] == r->ret;
    for (uint32_t s = 0; s < FX_SLOTS; s++) {
        const int16_t *g = song.g + G_MFXTYPE + s * FX_PARAMS;
        ok &= g[0] == r->fx[s].type && g[1] == r->fx[s].mix && g[2] == r->fx[s].rate && g[3] == r->fx[s].chr &&
              song.g[G_MFXTONE + s] == r->fx[s].tone;
        for (uint32_t k = 0; k < NPART; k++)
            ok &= trk[k].p[P_FXSEND + s] == DOCK_LINEUPS[i].part[k].p[P_FXSEND + s] &&
                  trk[k].p[P_FXTYPE + s * FX_PARAMS] == 0;
    }
    return ok;
}

/* host time of the mixer, ns per block, best of three: `voices` PROPHET voices on Keys, the shared bus as it is */
static double cost(uint32_t voices)
{
    static int32_t o[2u * CTL];
    static const uint8_t CHORD[6] = {48, 55, 60, 64, 67, 71};
    double best = 1e30;
    for (int run = 0; run < 3; run++) {
        for (uint32_t k = 0; k < NPART; k++) for (uint32_t v = 0; v < NVOICE; v++) trk[k].v[v].active = trk[k].v[v].gate = 0;
        trk[DOCK_KEYS].p[P_SUS] = 127;
        for (uint32_t v = 0; v < voices; v++) trk_note_on(&trk[DOCK_KEYS], CHORD[v], 100);
        for (uint32_t b = 0; b < 64; b++) mix_block(o, CTL);
        uint64_t t0 = now_ns();
        for (uint32_t b = 0; b < 4000; b++) mix_block(o, CTL);
        double ns = (double)(now_ns() - t0) / 4000.0;
        best = ns < best ? ns : best;
    }
    return best;
}
static uint32_t bus_bytes(void)
{
    uint32_t b = 0;
    for (uint32_t s = 0; s < FX_SLOTS; s++) b += resource[RES_FX0 + NPART * FX_SLOTS + s].size;
    return b;
}

int main(void)
{
    web_boot();
    run_ms(300);
    rcheck("power-on: Yacht's room with Yacht's sounds", dock_lineup == 0 && recalls(0));
    /* SELECT on HOME: every Line-up recalls its room, and back round */
    {
        int ok = 1;
        for (uint32_t i = 1; i <= DOCK_NLINEUPS; i++) { dock_lineup_turn(1); run_ms(30); ok &= recalls(i % DOCK_NLINEUPS); }
        dock_lineup_turn(-1); run_ms(30); ok &= recalls(DOCK_NLINEUPS - 1u);
        rcheck("SELECT steps the Line-ups, each recalling its room (and back round)", ok && dock_lineup == DOCK_NLINEUPS - 1u);
    }
    /* the rooms as the table says: echo times in range, the band's sends lower on the Bass */
    {
        int ok = 1;
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) {
            const dock_room_t *r = &DOCK_LINEUPS[i].room;
            for (uint32_t s = 0; s < FX_SLOTS; s++) ok &= r->fx[s].rate <= 127 && r->fx[s].chr <= 127 && r->fx[s].mix <= 127;
            const int16_t *b = DOCK_LINEUPS[i].part[DOCK_BASS].p, *kk = DOCK_LINEUPS[i].part[DOCK_KEYS].p,
                          *l = DOCK_LINEUPS[i].part[DOCK_LEAD].p;
            ok &= b[P_FXSEND] <= kk[P_FXSEND] && b[P_FXSEND] <= l[P_FXSEND];
            for (uint32_t k = 0; k < NPART; k++)
                for (uint32_t s = 0; s < FX_SLOTS; s++) ok &= DOCK_LINEUPS[i].part[k].p[P_FXTYPE + s * FX_PARAMS] == 0;
        }
        rcheck("every room's values in range; the Bass sends less than Keys and Lead; no inserts", ok);
    }
    /* each room through the whole band: finite, unclipped, heard; and the silence after */
    uint32_t bytes[DOCK_NLINEUPS], used[DOCK_NLINEUPS];
    for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) {
        take_t dry = {0}, dtail = {0}, wet = {0}, wtail = {0}, after = {0};
        dock_lineup_load(i);
        song.g[G_FXRETURN] = 0;                              /* the band dry (the room's return faded out) */
        run_ms(400);
        band(&dry, &dtail);
        run_ms(4000);
        dock_lineup_load(i);                                 /* and through its room */
        run_ms(400);
        band(&wet, &wtail);
        bytes[i] = bus_bytes();
        used[i] = resource_used();                           /* the whole audio arena: the engines and the room */
        run_ms(10000);                                       /* every tail gone */
        take(&after, 500);
        double add = db(&wet) - db(&dry), heard = db(&wtail) - db(&wet);
        printf("    %-6s plays %.1f dB (dry %.1f), peak %.3f; its tail %.1f dB under the band (dry: %.1f dB); after 10 s %.1f dB;"
               " bus %u B\n", DOCK_LINEUPS[i].name, db(&wet), db(&dry), wet.peak, -heard, db(&dtail), db(&after), bytes[i]);
        char what[96];
        snprintf(what, sizeof what, "%s: finite and below clipping through its room", DOCK_LINEUPS[i].name);
        rcheck(what, !wet.bad && !wtail.bad && wet.peak < 0.999f);
        snprintf(what, sizeof what, "%s: the room is heard (its tail within 40 dB of the band, 10 over dry)",
                 DOCK_LINEUPS[i].name);
        rcheck(what, heard >= -40.0 && db(&wtail) >= db(&dtail) + 10.0);
        snprintf(what, sizeof what, "%s: the room adds no more than 3 dB to the band playing", DOCK_LINEUPS[i].name);
        rcheck(what, add <= 3.0);
        snprintf(what, sizeof what, "%s: silence 10 s later (below -100 dB)", DOCK_LINEUPS[i].name);
        rcheck(what, db(&after) < -100.0);
    }
    /* the SPRING alone: a Lead staccato into Hot's room rings on after the note */
    {
        take_t a = {0}, b = {0};
        dock_lineup_load(2);
        for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_FXSEND] = 0, trk[k].p[P_FXSEND2] = 0;
        trk[DOCK_LEAD].p[P_FXSEND] = 127;                    /* the spring only */
        run_ms(500);
        web_midi(0x90 | DOCK_LEAD, 64, 110); run_ms(60); web_midi(0x80 | DOCK_LEAD, 64, 0);
        take(&a, 300); take(&b, 300);
        printf("    SPRING: 0.06-0.36 s after a 60 ms note %.1f dB, 0.36-0.66 s %.1f dB\n", db(&a), db(&b));
        rcheck("Hot's SPRING rings on after a short note (0.3 s later still above -60 dB)", db(&b) > -60.0 && !a.bad);
        run_ms(10000);
    }
    /* cost: the shared bus against PROPHET voices (host time; the device figure needs the JieLi toolchain) */
    {
        dock_lineup_load(0);
        for (uint32_t k = 0; k < NPART; k++) if (k != DOCK_KEYS) { set_engine_of(&trk[k], ENGI_PROPHET); apply_preset_to(&trk[k], 0); }
        set_engine_of(&trk[DOCK_KEYS], ENGI_PROPHET); apply_preset_to(&trk[DOCK_KEYS], 0);
        run_ms(300);
        int16_t keep[G_COUNT];
        memcpy(keep, song.g, sizeof keep);
        for (uint32_t s = 0; s < FX_SLOTS; s++) song.g[G_MFXTYPE + s * FX_PARAMS] = 0;
        run_ms(200);
        double idle = cost(0), p1 = cost(1) - idle, p6 = cost(6) - idle;
        double room[DOCK_NLINEUPS];
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) {
            dock_room_apply(&DOCK_LINEUPS[i].room);
            for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_FXSEND] = 127;   /* (the bus runs whatever it is sent) */
            run_ms(200);
            room[i] = cost(0) - idle;
        }
        printf("    host ns per %u-sample block: idle mix %.0f; PROPHET 1 voice %.0f, 6 voices %.0f (%.0f a voice)\n",
               CTL, idle, p1, p6, p6 / 6);
        printf("    the shared bus, ns a block (x a PROPHET voice):");
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) printf(" %s %.0f (%.2f)", DOCK_LINEUPS[i].name, room[i], room[i] / (p6 / 6));
        printf("\n    bus RAM (delay and reverb buffers, from the audio arena):");
        uint32_t most = 0;
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) { printf(" %s %u B", DOCK_LINEUPS[i].name, bytes[i]); most = bytes[i] > most ? bytes[i] : most; }
        printf("; fx_state %u B static\n    the audio arena in use, engines and room:", (uint32_t)sizeof fx_state);
        uint32_t top = 0;
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) { printf(" %s %u B", DOCK_LINEUPS[i].name, used[i]); top = used[i] > top ? used[i] : top; }
        printf("\n");
        rcheck("every Line-up's engines and room together fit the 48 KiB arena floor", top <= 48u * 1024u);
        double worst = 0;
        for (uint32_t i = 0; i < DOCK_NLINEUPS; i++) worst = room[i] > worst ? room[i] : worst;
        rcheck("the dearest room costs no more than two PROPHET voices (host)", worst <= 2.0 * (p6 / 6));
        rcheck("the largest room's buffers fit 16 KiB", most <= 16384u);
        memcpy(song.g, keep, sizeof keep);
    }
    printf("room: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
