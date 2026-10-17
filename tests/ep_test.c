/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * EP (firmware/src/eng_ep.c, FRodhes' Rhodes) on the host: every sound plays, stays bounded and below clipping; the
 * bark (the pickup's harmonics) grows with velocity; key-up damps the tine and the voice ends; the pitch follows the
 * key and a bend; the Suitcase's TREM is stereo, WURLY's mono; Suitcase is the Keys at power-on; the cost of an EP
 * voice against a PROPHET and a JUNO voice (host time, the same notes) and the size of its tables.
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/ep_test tests/ep_test.c -lm
 *   build/host/ep_test [WAV_DIR]   (WAV_DIR: each sound as a stereo WAV, to listen to) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("ep: %-68s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

typedef struct { double l2, r2, s2, d2; int32_t peak; uint32_t n, xings; int bounded; int32_t lp, lpp; } take_t;

static int ep_bounded(uint32_t part)              /* the resonators never sit at their saturation */
{
    const ep_part_t *P = ep_part(part);
    if (!P) return 1;
    for (uint32_t i = 0; i < EP_POLY; i++)
        for (uint32_t m = 0; m < RH_NM; m++)
            if (rh_iabs(P->v[i].m[m].x) >= (1 << 28) - 1 || rh_iabs(P->v[i].m[m].y) >= (1 << 28) - 1) return 0;
    return 1;
}

/* the left channel captured while cap_live (up to 1 s), and its power at f Hz (Goertzel, Hann window) */
static float cap[44100];
static uint32_t cap_n;
static int cap_live;
static double goertzel(double f)
{
    double w = 2.0 * M_PI * f / FS, c = 2.0 * cos(w), s1 = 0.0, s2 = 0.0;
    for (uint32_t i = 0; i < cap_n; i++) {
        double x = cap[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (cap_n - 1u))), s0 = x + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
/* renders blocks of the mix, gathers L, R, side (L - R), first-difference energy, the peak and the rising zero
 * crossings of a gently low-passed left channel (the pitch); w: a WAV or 0 */
static void render(take_t *tk, uint32_t blocks, uint32_t part, FILE *w)
{
    static int32_t o[2u * CTL];
    int32_t prev = 0;
    while (blocks--) {
        mix_block(o, CTL);
        tk->bounded &= ep_bounded(part);
        for (uint32_t i = 0; i < CTL; i++) {
            int32_t l = o[2u * i], r = o[2u * i + 1u], a = l < 0 ? -l : l, b = r < 0 ? -r : r;
            tk->l2 += (double)l * l;
            tk->r2 += (double)r * r;
            tk->s2 += (double)(l - r) * (l - r);
            tk->d2 += (double)(l - prev) * (l - prev);
            prev = l;
            tk->lp += (l - tk->lp) >> 3;          /* ~900 Hz one-pole: the fundamental of A4 well kept */
            tk->xings += tk->lpp < 0 && tk->lp >= 0;
            tk->lpp = tk->lp;
            tk->peak = a > tk->peak ? a : tk->peak;
            tk->peak = b > tk->peak ? b : tk->peak;
            tk->n++;
            if (cap_live && cap_n < 44100u) cap[cap_n++] = (float)l;
            if (w) {
                int16_t s[2] = {(int16_t)clamp(l, -32768, 32767), (int16_t)clamp(r, -32768, 32767)};
                fwrite(s, 2, 2, w);
            }
        }
    }
}
static take_t take0(void) { take_t tk = {0}; tk.bounded = 1; return tk; }

static uint32_t sounding(uint32_t part)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++) n += trk[part].v[i].active;
    return n;
}
static void silence(void)                         /* every voice off and gone */
{
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t i = 0; i < NVOICE; i++)
            trk[k].v[i].active = trk[k].v[i].gate = 0;
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    take_t tk = take0();
    render(&tk, 8, 0, 0);
}

static const uint8_t CHORD[6] = {48, 55, 60, 64, 67, 71};
/* part's sound: EP preset k (or another engine's), notes at vel held for `hold` blocks then released for `rel` */
static take_t play(uint32_t part, uint32_t e, uint32_t k, uint32_t nnotes, uint32_t vel, uint32_t hold, uint32_t rel,
                   FILE *w)
{
    take_t tk = take0();
    track_t *t = &trk[part];
    silence();
    host_preset(t, e, k);
    if (part == DOCK_LEAD || part == DOCK_BASS) t->p[P_VOICE] = V_LEGATO;
    for (uint32_t i = 0; i < nnotes; i++)
        trk_note_on(t, nnotes == 1u ? 57u : CHORD[i], vel);
    render(&tk, hold, part, w);
    for (uint32_t i = 0; i < nnotes; i++)
        trk_note_off(t, nnotes == 1u ? 57u : CHORD[i]);
    render(&tk, rel, part, w);
    return tk;
}

static FILE *wav_open(const char *dir, const char *name)
{
    char path[512];
    if (!dir) return 0;
    snprintf(path, sizeof path, "%s/ep_%s.wav", dir, name);
    for (char *c = path + strlen(dir); *c; c++) if (*c == ' ') *c = '_';
    FILE *f = fopen(path, "wb");
    if (f) wav_hdr(f, 0);
    return f;
}
static void wav_close(FILE *f)
{
    if (!f) return;
    long bytes = ftell(f) - 44;
    fseek(f, 0, SEEK_SET);
    wav_hdr(f, (uint32_t)(bytes / 4));
    fclose(f);
}

/* host time of `voices` voices of engine e (preset k) on Keys, ns per block, best of three (the EP's notes are
 * restruck now and then so they keep ringing at full cost: no mode goes to sleep) */
static double cost(uint32_t e, uint32_t k, uint32_t voices)
{
    static int32_t o[2u * CTL];
    double best = 1e30;
    for (int run = 0; run < 3; run++) {
        silence();
        host_preset(&trk[DOCK_KEYS], e, k);
        trk[DOCK_KEYS].p[P_SUS] = 127;
        for (uint32_t i = 0; i < voices; i++) trk_note_on(&trk[DOCK_KEYS], CHORD[i], 100);
        for (uint32_t b = 0; b < 64; b++) mix_block(o, CTL);
        uint64_t t0 = now_ns();
        for (uint32_t b = 0; b < 4000; b++) {
            if (e == ENGI_EP && b % 1000u == 999u)
                for (uint32_t i = 0; i < voices; i++) trk_note_on(&trk[DOCK_KEYS], CHORD[i], 100);
            mix_block(o, CTL);
        }
        double ns = (double)(now_ns() - t0) / 4000.0;
        best = ns < best ? ns : best;
    }
    return best;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    char name[21];
    host_tracks_init();
    for (uint32_t k = 0; k < NPART; k++)
        host_power_on_sound(&trk[k], k);

    str_cpy(name, eng_preset(ENGINES[TRK_DEF[DOCK_KEYS][0]], TRK_DEF[DOCK_KEYS][1])->name, 21);
    check("power-on (Yacht): the Keys play EP SUITCASE", trk[DOCK_KEYS].engine == ENGI_EP && str_eq(name, "SUITCASE"));
    check("EP can be picked (PRESETS, the editor)", eng_ok(ENGI_EP) && eng_rank(ENGI_EP) < NENG_SHOWN &&
                                                    eng_vis(eng_rank(ENGI_EP)) == ENGI_EP);
    check("EP plays on Bass, Keys and Lead, not on the drummer", DOCK_ENGINE_OK(DOCK_BASS, ENGI_EP) &&
          DOCK_ENGINE_OK(DOCK_KEYS, ENGI_EP) && DOCK_ENGINE_OK(DOCK_LEAD, ENGI_EP) && !DOCK_ENGINE_OK(DOCK_DRUMS, ENGI_EP));

    /* every sound, a six-note chord at 100 and at 127: it sounds, stays bounded, below clipping */
    int all_sound = 1, all_bounded = 1, all_clean = 1;
    for (uint32_t k = 0; k < EP_NPRESETS; k++) {
        FILE *w = wav_open(dir, EP_PATCH[k].name);
        take_t tk = play(DOCK_KEYS, ENGI_EP, k, 6, 100, (uint32_t)(2.5 * FS / CTL), (uint32_t)(1.0 * FS / CTL), w);
        take_t hard = play(DOCK_KEYS, ENGI_EP, k, 6, 127, (uint32_t)(0.5 * FS / CTL), (uint32_t)(0.2 * FS / CTL), w);
        wav_close(w);
        double rms = sqrt(tk.l2 / tk.n);
        printf("    %-12s v100: peak %5d  rms %7.1f  width %5.3f | v127: peak %5d\n", EP_PATCH[k].name, tk.peak, rms,
               tk.l2 + tk.r2 > 0 ? sqrt(tk.s2 / (tk.l2 + tk.r2)) : 0.0, hard.peak);
        all_sound &= rms > 300.0;
        all_bounded &= tk.bounded && hard.bounded;
        all_clean &= tk.peak < 32000 && hard.peak < 32000;
    }
    check("every sound plays (rms above -40 dBFS)", all_sound);
    check("no resonator runs into its limit in any sound", all_bounded);
    check("no sound clips, six notes at 100 and at 127 (peak below 32000)", all_clean);

    /* the bark: the pickup's 2nd and 3rd harmonics against the fundamental (A3, 30 .. 300 ms) grow with velocity */
    {
        double br[3];
        const uint32_t vel[3] = {30, 80, 127};
        for (uint32_t i = 0; i < 3u; i++) {
            take_t skip = take0();
            silence();
            host_preset(&trk[DOCK_KEYS], ENGI_EP, 0);
            trk_note_on(&trk[DOCK_KEYS], 57, vel[i]);
            render(&skip, (uint32_t)(0.03 * FS / CTL), DOCK_KEYS, 0);
            cap_n = 0;
            cap_live = 1;
            render(&skip, (uint32_t)(0.27 * FS / CTL), DOCK_KEYS, 0);
            cap_live = 0;
            double h1 = goertzel(220.0), h2 = goertzel(440.0), h3 = goertzel(660.0);
            br[i] = (h2 + h3) / h1;
            printf("    STAGE A3 v%-3u H2/H1 %6.1f dB  H3/H1 %6.1f dB\n", vel[i], 10.0 * log10(h2 / h1),
                   10.0 * log10(h3 / h1));
        }
        check("bark: more H2 + H3 against H1 at velocity 80 than 30, at 127 than 80", br[1] > br[0] * 1.25 &&
              br[2] > br[1] * 1.25);
    }
    /* BARK 127 against BARK 0 at the same velocity */
    {
        double b[2];
        for (uint32_t i = 0; i < 2u; i++) {
            take_t tk = take0();
            silence();
            host_preset(&trk[DOCK_KEYS], ENGI_EP, 0);
            trk[DOCK_KEYS].p[P_E1] = i ? 127 : 0;
            trk_note_on(&trk[DOCK_KEYS], 57, 100);
            render(&tk, (uint32_t)(0.3 * FS / CTL), DOCK_KEYS, 0);
            b[i] = tk.d2 / tk.l2;
        }
        check("BARK 127 is brighter than BARK 0", b[1] > b[0] * 1.3);
    }

    /* key-up damps: the same note held and released, the energy 0.2 .. 0.5 s after the key-up */
    {
        take_t held = take0(), rel = take0(), skip = take0();
        uint32_t a = (uint32_t)(0.5 * FS / CTL), d = (uint32_t)(0.2 * FS / CTL), m = (uint32_t)(0.3 * FS / CTL);
        silence();
        host_preset(&trk[DOCK_KEYS], ENGI_EP, 0);
        trk_note_on(&trk[DOCK_KEYS], 57, 100);
        render(&skip, a + d, DOCK_KEYS, 0);
        render(&held, m, DOCK_KEYS, 0);
        silence();
        trk_note_on(&trk[DOCK_KEYS], 57, 100);
        render(&skip, a, DOCK_KEYS, 0);
        trk_note_off(&trk[DOCK_KEYS], 57);
        render(&skip, d, DOCK_KEYS, 0);
        render(&rel, m, DOCK_KEYS, 0);
        double db = 10.0 * log10((rel.l2 + 1.0) / (held.l2 + 1.0));
        printf("    released / held, 0.2 .. 0.5 s after key-up: %.1f dB\n", db);
        check("key-up damps the tine (at least 20 dB down 0.2 s later)", db < -20.0);
        render(&skip, (uint32_t)(1.5 * FS / CTL), DOCK_KEYS, 0);
        check("the released voice ends (its modes die away)", sounding(DOCK_KEYS) == 0);
    }

    /* pitch: A4 at 440 Hz; a +2 semitone bend moves it to 493.9 Hz */
    {
        double hz[2];
        for (uint32_t i = 0; i < 2u; i++) {
            take_t tk = take0(), skip = take0();
            silence();
            host_preset(&trk[DOCK_KEYS], ENGI_EP, 2);             /* STAGE 77 */
            if (i) midi_bend_q8[DOCK_KEYS] = midi_bend_target[DOCK_KEYS] = 2 * 256;
            trk_note_on(&trk[DOCK_KEYS], 69, 64);
            render(&skip, (uint32_t)(0.2 * FS / CTL), DOCK_KEYS, 0);
            render(&tk, (uint32_t)(1.0 * FS / CTL), DOCK_KEYS, 0);
            hz[i] = tk.xings * (double)FS / tk.n;
        }
        printf("    A4: %.1f Hz, bent +2: %.1f Hz\n", hz[0], hz[1]);
        check("A4 sounds at 440 Hz (within 0.5 %)", fabs(hz[0] / 440.0 - 1.0) < 0.005);
        check("a +2 semitone bend sounds at 493.9 Hz (within 0.5 %)", fabs(hz[1] / 493.88 - 1.0) < 0.005);
    }

    /* TREM: Suitcase stereo (L - R), WURLY mono; TREM 0 mono */
    {
        take_t s = play(DOCK_KEYS, ENGI_EP, EP_SUITCASE, 3, 100, (uint32_t)(1.5 * FS / CTL), 0, 0);
        take_t wu = play(DOCK_KEYS, ENGI_EP, EP_WURLY, 3, 100, (uint32_t)(1.5 * FS / CTL), 0, 0);
        take_t st = play(DOCK_KEYS, ENGI_EP, 2, 3, 100, (uint32_t)(1.5 * FS / CTL), 0, 0);
        double ws = sqrt(s.s2 / (s.l2 + s.r2)), ww = sqrt(wu.s2 / (wu.l2 + wu.r2)), w0 = sqrt(st.s2 / (st.l2 + st.r2));
        printf("    width: SUITCASE %.3f, WURLY %.3f, STAGE 77 %.3f\n", ws, ww, w0);
        check("SUITCASE's TREM pans (stereo)", ws > 0.15);
        check("WURLY's tremolo and STAGE 77 (no TREM) are mono", ww < 0.002 && w0 < 0.002);
    }

    /* more notes than voices, fast: the steals fade, nothing clips; the mono Lead restrikes a legato note */
    {
        take_t tk = take0();
        silence();
        host_preset(&trk[DOCK_KEYS], ENGI_EP, 0);
        for (uint32_t i = 0; i < 24u; i++) {
            trk_note_on(&trk[DOCK_KEYS], 40u + (i * 7u) % 36u, 127);
            render(&tk, 3, DOCK_KEYS, 0);
        }
        render(&tk, 200, DOCK_KEYS, 0);
        check("24 notes on 8 voices: bounded, below clipping", tk.bounded && tk.peak < 32000);
        take_t a = take0(), b = take0();
        silence();
        host_preset(&trk[DOCK_LEAD], ENGI_EP, 0);
        trk[DOCK_LEAD].p[P_VOICE] = V_LEGATO;
        trk_note_on(&trk[DOCK_LEAD], 57, 100);
        render(&a, (uint32_t)(0.5 * FS / CTL), DOCK_LEAD, 0);
        trk_note_on(&trk[DOCK_LEAD], 64, 100);                    /* before 57's key-up: a slide */
        trk_note_off(&trk[DOCK_LEAD], 57);
        render(&b, (uint32_t)(0.1 * FS / CTL), DOCK_LEAD, 0);
        const ep_voice_t *v = &ep_part(DOCK_LEAD)->v[0];
        check("Lead legato: the new key strikes (it sounds, on key E4)", v->alive && v->note == 64 && b.peak > 2000);
    }

    /* cost: one and six voices, EP INIT against PROPHET INIT and JUNO INIT on Keys (the whole mix_block) */
    {
        double idle = cost(ENGI_EP, 0, 0);
        double e1 = cost(ENGI_EP, 0, 1) - idle, p1 = cost(ENGI_PROPHET, 0, 1) - idle, j1 = cost(ENGI_JUNO, 0, 1) - idle;
        double e6 = cost(ENGI_EP, 0, 6) - idle, p6 = cost(ENGI_PROPHET, 0, 6) - idle, j6 = cost(ENGI_JUNO, 0, 6) - idle;
        double es = cost(ENGI_EP, EP_SUITCASE, 6) - idle;
        printf("    host ns per 32-sample block: idle mix %.0f; EP 1 voice %.0f, 6 voices %.0f (SUITCASE %.0f);"
               " PROPHET %.0f, %.0f; JUNO %.0f, %.0f\n", idle, e1, e6, es, p1, p6, j1, j6);
        printf("    EP / PROPHET per voice: %.2f (1 voice), %.2f (6); EP / JUNO: %.2f, %.2f; units: PROPHET %u, JUNO %u,"
               " EP %u\n", e1 / p1, e6 / p6, e1 / j1, e6 / j6, p5_units(&trk[DOCK_KEYS]), juno_units(&trk[DOCK_KEYS]),
               ep_units(&trk[DOCK_KEYS]));
        check("an EP voice costs no more than a PROPHET voice on the host", e6 <= p6 * 1.05);
    }
    /* the tables (flash on the device) and the state (RAM, per part) */
    {
        size_t keys = sizeof RH_KEYS, pulse = sizeof RH_PULSE, rate = sizeof RH_PRATE + sizeof RH_PAMP,
               pick = sizeof RH_PHI + sizeof RH_SLOPE,
               rest = sizeof RH_VPOS_I + sizeof RH_VPOS_F + sizeof RH_VLAY_I + sizeof RH_VLAY_W + sizeof RH_RLC +
                      sizeof RH_BODY + sizeof RH_TYPE + sizeof RH_DECAY_K;
        printf("    tables: keys %zu, pulses %zu, rates %zu, pickup %zu, the rest %zu: %zu B (%.1f KiB); state %zu B a"
               " part\n", keys, pulse, rate, pick, rest, keys + pulse + rate + pick + rest,
               (keys + pulse + rate + pick + rest) / 1024.0, sizeof(ep_part_t));
    }
    printf("ep: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
