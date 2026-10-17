/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's organ (WHEEL, firmware/src/eng_wheel.c) and PLUCK (PHYS, firmware/src/eng_phys.c) on the host: both can be
 * picked on Bass, Keys and Lead and not on the drummer; every sound plays, bounded and below clipping; the organ's
 * pitch, rotary, drive and percussion; the combo organs' vibrato; the upright's short woody note against the surf
 * twang's long bright one; the pluck's pitch; the cost of a voice of each against PROPHET, JUNO and EP (host time,
 * the same notes); the band's worst-case engine memory with PLUCK on all three members.
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/organ_pluck_test tests/organ_pluck_test.c -lm
 *   build/host/organ_pluck_test [WAV_DIR]   (WAV_DIR: each sound as a stereo WAV, to listen to) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("organ/pluck: %-62s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

typedef struct { double l2, r2, d2; int32_t peak; uint32_t n; } take_t;

/* the left channel captured while cap_live (up to 2 s) */
static float cap[88200];
static uint32_t cap_n;
static int cap_live;
static double goertzel(const float *x, uint32_t n, double f)    /* power at f Hz, Hann window */
{
    double w = 2.0 * M_PI * f / FS, c = 2.0 * cos(w), s1 = 0.0, s2 = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double s0 = x[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1u))) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
static double pitch_hz(const float *x, uint32_t n)              /* autocorrelation peak, 25 .. 1000 Hz, interpolated */
{
    uint32_t lo = FS / 1000, hi = FS / 25, best = lo;
    double r[2000], top = -1e300;
    for (uint32_t lag = lo; lag <= hi && lag < 2000u; lag++) {
        double s = 0.0;
        for (uint32_t i = 0; i + lag < n; i++) s += (double)x[i] * x[i + lag];
        r[lag] = s / (n - lag);
    }
    for (uint32_t lag = lo + 1u; lag < hi && lag < 1999u; lag++)
        if (r[lag] > r[lag - 1u] && r[lag] >= r[lag + 1u] && r[lag] > top * 1.02) { top = r[lag]; best = lag; }
    double a = r[best - 1u], b = r[best], c = r[best + 1u], d = a - 2.0 * b + c;
    return FS / (best + (d != 0.0 ? 0.5 * (a - c) / d : 0.0));
}

static void render(take_t *tk, uint32_t blocks, FILE *w)
{
    static int32_t o[2u * CTL];
    int32_t prev = 0;
    while (blocks--) {
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            int32_t l = o[2u * i], r = o[2u * i + 1u], a = l < 0 ? -l : l, b = r < 0 ? -r : r;
            tk->l2 += (double)l * l;
            tk->r2 += (double)r * r;
            tk->d2 += (double)(l - prev) * (l - prev);
            prev = l;
            tk->peak = a > tk->peak ? a : tk->peak;
            tk->peak = b > tk->peak ? b : tk->peak;
            tk->n++;
            if (cap_live && cap_n < NELEM(cap)) cap[cap_n++] = (float)l;
            if (w) {
                int16_t s[2] = {(int16_t)clamp(l, -32768, 32767), (int16_t)clamp(r, -32768, 32767)};
                fwrite(s, 2, 2, w);
            }
        }
    }
}
#define BLK(sec) ((uint32_t)((sec) * FS / CTL))

static void silence(void)                                       /* every voice off and gone */
{
    take_t tk = {0};
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t i = 0; i < NVOICE; i++)
            trk[k].v[i].active = trk[k].v[i].gate = 0;
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    render(&tk, 8, 0);
}
static uint32_t sounding(uint32_t part)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++) n += trk[part].v[i].active;
    return n;
}
static void sound(uint32_t part, uint32_t e, uint32_t k)
{
    static const uint8_t LFO[] = {P_LWAVE, P_LRATE, P_LFADE, P_LD_PIT, P_LD_FLT, P_LD_SHP};
    silence();
    for (uint32_t i = 0; i < NELEM(LFO); i++)                   /* (as ui.c apply_preset_to: the LFO to its defaults) */
        trk[part].p[LFO[i]] = TP[LFO[i]].def;
    host_preset(&trk[part], e, k);
    if (part == DOCK_LEAD || part == DOCK_BASS) trk[part].p[P_VOICE] = V_LEGATO;
}

static const uint8_t CHORD[6] = {48, 55, 60, 64, 67, 71};
static take_t play(uint32_t part, uint32_t e, uint32_t k, uint32_t nnotes, uint32_t vel, uint32_t hold, uint32_t rel,
                   FILE *w)
{
    take_t tk = {0};
    sound(part, e, k);
    for (uint32_t i = 0; i < nnotes; i++) trk_note_on(&trk[part], CHORD[i], vel);
    render(&tk, hold, w);
    for (uint32_t i = 0; i < nnotes; i++) trk_note_off(&trk[part], CHORD[i]);
    render(&tk, rel, w);
    return tk;
}
/* one note: the first `skip` seconds, then `sec` seconds captured */
static take_t capture(uint32_t part, uint32_t note, uint32_t vel, double skip, double sec)
{
    take_t a = {0}, tk = {0};
    trk_note_on(&trk[part], note, vel);
    render(&a, BLK(skip), 0);
    cap_n = 0;
    cap_live = 1;
    render(&tk, BLK(sec), 0);
    cap_live = 0;
    return tk;
}

static FILE *wav_open(const char *dir, const char *eng, const char *name)
{
    char path[512];
    if (!dir) return 0;
    snprintf(path, sizeof path, "%s/%s_%s.wav", dir, eng, name);
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

/* host time of `voices` voices of engine e (preset k) on Keys, ns per block, best of three; plucks and pianos are
 * restruck now and then so they keep sounding at full cost */
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
            if ((e == ENGI_EP || e == ENGI_PHYS) && b % 500u == 499u)
                for (uint32_t i = 0; i < voices; i++) trk_note_on(&trk[DOCK_KEYS], CHORD[i], 100);
            mix_block(o, CTL);
        }
        double ns = (double)(now_ns() - t0) / 4000.0;
        best = ns < best ? ns : best;
    }
    return best;
}

/* the part's engine memory for engine e (the drummer: its largest kit) */
static uint32_t state_of(uint32_t e)
{
    if (e != ENGI_DRUM) return eng_state_size(e);
    uint32_t a = (uint32_t)sizeof(drum909_part_t), b = (uint32_t)(sizeof(drum_lane_t) * DV_NLANE), c = dsk_bytes();
    return a > b ? (a > c ? a : c) : (b > c ? b : c);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    const engine_t *W = ENGINES[ENGI_WHEEL], *P = ENGINES[ENGI_PHYS];
    host_tracks_init();
    for (uint32_t k = 0; k < NPART; k++) host_power_on_sound(&trk[k], k);

    check("WHEEL and PLUCK can be picked (PRESETS, the editor)",
          eng_ok(ENGI_WHEEL) && eng_ok(ENGI_PHYS) && eng_vis(eng_rank(ENGI_WHEEL)) == ENGI_WHEEL &&
          eng_vis(eng_rank(ENGI_PHYS)) == ENGI_PHYS && W == &ENG_WHEEL && P == &ENG_PHYS);
    check("PHYS shows as PLUCK", str_eq(P->name, "PLUCK") && str_eq(W->name, "WHEEL"));
    check("both play on Bass, Keys and Lead, not on the drummer",
          DOCK_ENGINE_OK(DOCK_BASS, ENGI_WHEEL) && DOCK_ENGINE_OK(DOCK_KEYS, ENGI_WHEEL) &&
          DOCK_ENGINE_OK(DOCK_LEAD, ENGI_WHEEL) && !DOCK_ENGINE_OK(DOCK_DRUMS, ENGI_WHEEL) &&
          DOCK_ENGINE_OK(DOCK_BASS, ENGI_PHYS) && DOCK_ENGINE_OK(DOCK_KEYS, ENGI_PHYS) &&
          DOCK_ENGINE_OK(DOCK_LEAD, ENGI_PHYS) && !DOCK_ENGINE_OK(DOCK_DRUMS, ENGI_PHYS));
    check("the other retired extras stay out (PHASE, VOICE, TRIO, NOISE)",
          !eng_ok(2) && !eng_ok(5) && !eng_ok(6) && !eng_ok(11));
    check("Dock's presets where the Line-ups look for them",
          str_eq(W->presets[WHEEL_MOD_HAMMOND].name, "MOD HAMMOND") && str_eq(W->presets[WHEEL_VOX].name, "VOX COMBO") &&
          str_eq(P->presets[PHYS_UPRIGHT].name, "UPRIGHT") && str_eq(P->presets[PHYS_SURF].name, "SURF TWANG"));

    /* every sound: WHEEL six notes, PLUCK three (its polyphony), at 100 and 127; and alone on the mono Bass */
    {
        int ok_sound = 1, ok_clean = 1;
        for (uint32_t e = 0; e < 2u; e++) {
            const engine_t *en = e ? P : W;
            uint32_t ei = e ? ENGI_PHYS : ENGI_WHEEL, nn = e ? PHYS_POLY : 6u;
            for (uint32_t k = 0; k < en->npresets; k++) {
                FILE *w = wav_open(dir, e ? "pluck" : "wheel", en->presets[k].name);
                take_t tk = play(DOCK_KEYS, ei, k, nn, 100, BLK(1.5), BLK(0.6), w);
                take_t hard = play(DOCK_KEYS, ei, k, nn, 127, BLK(0.5), BLK(0.2), w);
                take_t bass = play(DOCK_BASS, ei, k, 1, 127, BLK(0.5), BLK(0.2), w);
                wav_close(w);
                double rms = sqrt(tk.l2 / tk.n);
                printf("    %-5s %-12s v100: peak %5d rms %7.1f | v127: peak %5d | Bass: peak %5d\n", e ? "PLUCK" : "WHEEL",
                       en->presets[k].name, tk.peak, rms, hard.peak, bass.peak);
                ok_sound &= rms > 300.0 && bass.peak > 1000;
                ok_clean &= tk.peak < 32000 && hard.peak < 32000 && bass.peak < 32000;
            }
        }
        check("every sound plays (rms above -40 dBFS; on the Bass too)", ok_sound);
        check("no sound clips (chords at 100 and 127, peak below 32000)", ok_clean);
    }

    /* WHEEL: a plain 8' flute (no rotor, click, drive) sounds A4 at 440 Hz */
    sound(DOCK_KEYS, ENGI_WHEEL, 3);
    trk[DOCK_KEYS].p[P_E0] = 0;                                 /* FLUTE: 008000000 */
    trk[DOCK_KEYS].p[P_E2] = trk[DOCK_KEYS].p[P_E5] = trk[DOCK_KEYS].p[P_E6] = trk[DOCK_KEYS].p[P_E7] = 0;
    {
        capture(DOCK_KEYS, 69, 100, 0.1, 0.5);
        double hz = pitch_hz(cap, cap_n), h1 = goertzel(cap, cap_n, 440.0), h3 = goertzel(cap, cap_n, 1320.0);
        printf("    WHEEL FLUTE A4: %.1f Hz, H3/H1 %.1f dB\n", hz, 10.0 * log10(h3 / h1));
        check("WHEEL: A4 at 440 Hz (within 0.3 %)", fabs(hz / 440.0 - 1.0) < 0.003);
        /* DRV 127: the overdrive adds odd harmonics */
        silence();
        trk[DOCK_KEYS].p[P_E6] = 127;
        capture(DOCK_KEYS, 69, 100, 0.1, 0.5);
        double d1 = goertzel(cap, cap_n, 440.0), d3 = goertzel(cap, cap_n, 1320.0);
        printf("    WHEEL FLUTE DRV 127: H3/H1 %.1f dB\n", 10.0 * log10(d3 / d1));
        check("WHEEL: DRV adds harmonics (H3/H1 up 20 dB or more)", d3 / d1 > 100.0 * h3 / h1);
    }
    /* the rotary: the level's ripple (windows of 8 periods of C4 over 1.5 s) OFF, SLOW, FAST */
    {
        double cv[3];
        for (uint32_t r = 0; r < 3u; r++) {
            sound(DOCK_KEYS, ENGI_WHEEL, 3);
            trk[DOCK_KEYS].p[P_E0] = 0;                         /* FLUTE: 008000000 */
            trk[DOCK_KEYS].p[P_E2] = trk[DOCK_KEYS].p[P_E5] = trk[DOCK_KEYS].p[P_E6] = 0;
            trk[DOCK_KEYS].p[P_E7] = (int16_t)r;
            capture(DOCK_KEYS, 60, 100, 2.0, 1.5);              /* (the rotors up to speed) */
            double s = 0.0, s2 = 0.0;
            uint32_t win = (uint32_t)(8.0 * FS / 261.63 + 0.5), nw = cap_n / win;
            for (uint32_t w = 0; w < nw; w++) {
                double e = 0.0;
                for (uint32_t i = 0; i < win; i++) e += (double)cap[w * win + i] * cap[w * win + i];
                e = sqrt(e / win);
                s += e;
                s2 += e * e;
            }
            cv[r] = sqrt(s2 / nw - (s / nw) * (s / nw)) / (s / nw);
        }
        printf("    WHEEL rotary level ripple: OFF %.3f, SLOW %.3f, FAST %.3f\n", cv[0], cv[1], cv[2]);
        check("WHEEL: the rotor moves the level, FAST more than OFF", cv[2] > 4.0 * cv[0] && cv[2] > 0.05);
        check("  SLOW too", cv[1] > 2.0 * cv[0]);
    }
    /* PERC 2ND: the 4' (2nd harmonic) strikes and decays; a legato note gets none (single trigger) */
    {
        sound(DOCK_KEYS, ENGI_WHEEL, 3);
        trk[DOCK_KEYS].p[P_E0] = 0;                             /* FLUTE: the 8' alone, so the 4' is the percussion */
        trk[DOCK_KEYS].p[P_E2] = trk[DOCK_KEYS].p[P_E5] = trk[DOCK_KEYS].p[P_E6] = trk[DOCK_KEYS].p[P_E7] = 0;
        trk[DOCK_KEYS].p[P_E4] = 1;                             /* 2ND */
        capture(DOCK_KEYS, 57, 100, 0.0, 1.2);
        double early = goertzel(cap, 2205, 440.0) / goertzel(cap, 2205, 220.0);
        double late = goertzel(cap + 44100, 2205, 440.0) / goertzel(cap + 44100, 2205, 220.0);
        trk_note_on(&trk[DOCK_KEYS], 64, 100);                  /* while 57 is held: no new strike */
        take_t a = {0};
        cap_n = 0;
        cap_live = 1;
        render(&a, BLK(0.06), 0);
        cap_live = 0;
        double leg = goertzel(cap + 441, 2205, 659.26) / goertzel(cap + 441, 2205, 329.63);
        printf("    WHEEL PERC 2ND: H2/H1 first 50 ms %.1f dB, at 1 s %.1f dB; a legato note %.1f dB\n",
               10.0 * log10(early), 10.0 * log10(late), 10.0 * log10(leg));
        check("WHEEL: the percussion strikes and decays (20 dB or more)", early > 100.0 * late);
        check("  single trigger: a legato note has none", leg < early / 100.0);
    }
    /* the combo organs' vibrato; the Hammonds have none */
    {
        int none = 1;
        for (uint32_t k = 0; k < W->npresets; k++) {
            sound(DOCK_KEYS, ENGI_WHEEL, k);
            if (k != WHEEL_VOX && k != 9u) none &= trk[DOCK_KEYS].p[P_LD_PIT] == 0;
        }
        double hz[2] = {1e9, 0.0};
        sound(DOCK_LEAD, ENGI_WHEEL, 9);                        /* FARFISA */
        check("FARFISA and VOX COMBO: vibrato on the LFO; the others none",
              trk[DOCK_LEAD].p[P_LD_PIT] > 0 && none &&
              (sound(DOCK_LEAD, ENGI_WHEEL, WHEEL_VOX), trk[DOCK_LEAD].p[P_LD_PIT] > 0));
        sound(DOCK_LEAD, ENGI_WHEEL, 9);
        trk_note_on(&trk[DOCK_LEAD], 69, 100);
        for (uint32_t w = 0; w < 12u; w++) {                    /* 60 ms windows over 0.72 s: the pitch moves */
            take_t a = {0};
            cap_n = 0;
            cap_live = 1;
            render(&a, BLK(0.06), 0);
            cap_live = 0;
            double f = pitch_hz(cap, cap_n);
            hz[0] = f < hz[0] ? f : hz[0];
            hz[1] = f > hz[1] ? f : hz[1];
        }
        printf("    FARFISA A4 vibrato: %.1f .. %.1f Hz (%.0f cents)\n", hz[0], hz[1], 1200.0 * log2(hz[1] / hz[0]));
        check("  FARFISA's pitch swings 20 cents or more", 1200.0 * log2(hz[1] / hz[0]) > 20.0);
    }

    /* PLUCK: UPRIGHT on the Bass and SURF TWANG on the Lead, A2 (110 Hz) held: pitch, decay, brightness */
    {
        double hz[2], decay[2], brite[2];
        for (uint32_t i = 0; i < 2u; i++) {
            uint32_t part = i ? DOCK_LEAD : DOCK_BASS;
            double e0 = 0.0, e1 = 0.0, d0 = 0.0, x0 = 0.0;
            for (uint32_t r = 0; r < 4u; r++) {                 /* four plucks (each burst is random) */
                sound(part, ENGI_PHYS, i ? PHYS_SURF : PHYS_UPRIGHT);
                capture(part, 45, 100, 0.0, 1.0);
                if (!r) hz[i] = pitch_hz(cap + 4410, 8820);
                for (uint32_t j = 0; j < 4410u; j++) e0 += (double)cap[j] * cap[j];         /* 0 .. 0.1 s */
                for (uint32_t j = 17640; j < 26460u; j++) e1 += (double)cap[j] * cap[j] / 2;   /* 0.4 .. 0.6 s */
                for (uint32_t j = 1; j < 4410u; j++) { double d = cap[j] - cap[j - 1u]; d0 += d * d; x0 += (double)cap[j] * cap[j]; }
            }
            decay[i] = 10.0 * log10((e1 + 1.0) / (e0 + 1.0));
            brite[i] = d0 / (x0 + 1.0);
        }
        printf("    PLUCK A2: UPRIGHT %.2f Hz, 0.4-0.6 s %.1f dB, brightness %.4f | SURF TWANG %.2f Hz, 0.4-0.6 s %.1f dB,"
               " brightness %.4f\n", hz[0], decay[0], brite[0], hz[1], decay[1], brite[1]);
        check("PLUCK: A2 at 110 Hz, both (within 0.5 %)", fabs(hz[0] / 110.0 - 1.0) < 0.005 && fabs(hz[1] / 110.0 - 1.0) < 0.005);
        check("UPRIGHT is short: 20 dB or more down 0.4 .. 0.6 s later, key held", decay[0] < -20.0);
        check("SURF TWANG rings on: 10 dB longer than the upright", decay[1] > decay[0] + 10.0);
        check("SURF TWANG is brighter than the upright (twice the edge)", brite[1] > 2.0 * brite[0]);
        /* key-up: the upright stops */
        sound(DOCK_BASS, ENGI_PHYS, PHYS_UPRIGHT);
        take_t a = {0}, rel = {0};
        trk_note_on(&trk[DOCK_BASS], 45, 100);
        render(&a, BLK(0.1), 0);
        trk_note_off(&trk[DOCK_BASS], 45);
        render(&a, BLK(0.15), 0);
        render(&rel, BLK(0.1), 0);
        double db = 10.0 * log10((rel.l2 / rel.n + 1.0) / (a.l2 / a.n + 1.0));
        render(&a, BLK(1.0), 0);
        printf("    UPRIGHT key-up: %.1f dB 0.15 s later\n", db);
        check("UPRIGHT key-up: 20 dB down within 0.15 s, the voice ends", db < -20.0 && sounding(DOCK_BASS) == 0);
        /* the mono Bass: a slide plays the new note */
        sound(DOCK_BASS, ENGI_PHYS, PHYS_UPRIGHT);
        trk_note_on(&trk[DOCK_BASS], 45, 100);
        render(&a, BLK(0.2), 0);
        trk_note_on(&trk[DOCK_BASS], 52, 100);
        trk_note_off(&trk[DOCK_BASS], 45);
        cap_n = 0;
        cap_live = 1;
        render(&a, BLK(0.25), 0);
        cap_live = 0;
        double f = pitch_hz(cap + 2205, cap_n - 2205);
        printf("    UPRIGHT slide A2 -> E3: %.1f Hz\n", f);
        check("Bass legato: a slide sounds the new note (E3, 164.8 Hz)", fabs(f / 164.81 - 1.0) < 0.01 && sounding(DOCK_BASS) == 1);
    }

    /* cost: WHEEL (MOD HAMMOND) and PLUCK (UPRIGHT, SURF TWANG) on Keys against PROPHET, JUNO and EP INIT */
    {
        double idle = cost(ENGI_WHEEL, 0, 0);
        double w1 = cost(ENGI_WHEEL, WHEEL_MOD_HAMMOND, 1) - idle, w6 = cost(ENGI_WHEEL, WHEEL_MOD_HAMMOND, 6) - idle;
        double u1 = cost(ENGI_PHYS, PHYS_UPRIGHT, 1) - idle, s3 = cost(ENGI_PHYS, PHYS_SURF, 3) - idle;
        double p1 = cost(ENGI_PROPHET, 0, 1) - idle, p6 = cost(ENGI_PROPHET, 0, 6) - idle;
        double j1 = cost(ENGI_JUNO, 0, 1) - idle, j6 = cost(ENGI_JUNO, 0, 6) - idle;
        double e1 = cost(ENGI_EP, 0, 1) - idle, e6 = cost(ENGI_EP, 0, 6) - idle;
        printf("    host ns per 32-sample block: idle mix %.0f; WHEEL 1 voice %.0f, 6 %.0f; PLUCK 1 %.0f, 3 %.0f;"
               " PROPHET %.0f, %.0f; JUNO %.0f, %.0f; EP %.0f, %.0f\n", idle, w1, w6, u1, s3, p1, p6, j1, j6, e1, e6);
        printf("    per voice against PROPHET: WHEEL %.2f, PLUCK %.2f; JUNO %.2f; EP %.2f. Units: PROPHET %u, JUNO %u, EP %u,"
               " WHEEL %u, PLUCK %u\n", (w6 / 6) / (p6 / 6), (s3 / 3) / (p6 / 6), j6 / p6, e6 / p6,
               p5_units(&trk[DOCK_KEYS]), juno_units(&trk[DOCK_KEYS]), ep_units(&trk[DOCK_KEYS]),
               (host_preset(&trk[DOCK_KEYS], ENGI_WHEEL, 0), voice_units(&trk[DOCK_KEYS])),
               (host_preset(&trk[DOCK_KEYS], ENGI_PHYS, 0), voice_units(&trk[DOCK_KEYS])));
        check("a WHEEL voice and a PLUCK voice cost no more than a PROPHET voice", w6 <= p6 * 1.05 && s3 / 3 <= p6 / 6 * 1.05);
    }
    /* RAM: the band's worst case (the largest state each member may hold, PLUCK the largest) fits 48 KiB */
    {
        uint32_t tot = 0, big[NPART] = {0};
        for (uint32_t k = 0; k < NPART; k++) {
            for (uint32_t n = 0; n < NENG_SHOWN; n++) {
                uint32_t e = eng_vis(n), b = state_of(e);
                if (DOCK_ENGINE_OK(k, e) && b > big[k]) big[k] = b;
            }
            tot += big[k];
        }
        printf("    state: WHEEL %u B, PLUCK %u B a part; the band's worst case %u + %u + %u + drums %u = %u B\n",
               eng_state_size(ENGI_WHEEL), eng_state_size(ENGI_PHYS), big[0], big[1], big[2], big[3], tot);
        check("the band's worst-case engine memory fits 48 KiB", tot <= 48u * 1024u && big[0] == eng_state_size(ENGI_PHYS));
    }
    printf("organ/pluck: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
