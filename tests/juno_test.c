/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * JUNO (firmware/src/eng_juno.c) on the host: every sound plays, stays finite and below clipping; the filter darkens;
 * chorus I and II widen the part to stereo while OFF and I+II stay mono; Silk Lead is the Lead at power-on; and the
 * cost of a JUNO voice against a PROPHET voice (host time, the same notes).
 *   cc -O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/juno_test tests/juno_test.c -lm
 *   build/host/juno_test [WAV_DIR]   (WAV_DIR: each sound as a stereo WAV, to listen to) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("juno: %-66s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

typedef struct { double l2, r2, s2, d2; int32_t peak; uint32_t n; int finite; } take_t;

static int juno_finite(uint32_t part)
{
    const juno_part_t *jp = juno_part(part);
    uint32_t i, k;
    if (!jp) return 1;
    for (i = 0; i < JUNO_POLY; i++)
        for (k = 0; k < 4u; k++)
            if (!isfinite(jp->v[i].z[k]) || fabsf(jp->v[i].z[k]) > 100.0f) return 0;
    for (i = 0; i < JUNO_DL; i++)
        if (!isfinite(jp->dl[i])) return 0;
    return 1;
}

/* renders blocks of the mix, gathers L, R, side (L - R) and first-difference energy, the peak; w: a WAV or 0 */
static void render(take_t *tk, uint32_t blocks, uint32_t part, FILE *w)
{
    static int32_t o[2u * CTL];
    int32_t prev = 0;
    while (blocks--) {
        mix_block(o, CTL);
        tk->finite &= juno_finite(part);
        for (uint32_t i = 0; i < CTL; i++) {
            int32_t l = o[2u * i], r = o[2u * i + 1u], a = l < 0 ? -l : l, b = r < 0 ? -r : r;
            tk->l2 += (double)l * l;
            tk->r2 += (double)r * r;
            tk->s2 += (double)(l - r) * (l - r);
            tk->d2 += (double)(l - prev) * (l - prev);
            prev = l;
            tk->peak = a > tk->peak ? a : tk->peak;
            tk->peak = b > tk->peak ? b : tk->peak;
            tk->n++;
            if (w) {
                int16_t s[2] = {(int16_t)clamp(l, -32768, 32767), (int16_t)clamp(r, -32768, 32767)};
                fwrite(s, 2, 2, w);
            }
        }
    }
}

static void silence(void)                         /* every voice off and gone */
{
    for (uint32_t k = 0; k < NPART; k++)
        for (uint32_t i = 0; i < NVOICE; i++)
            trk[k].v[i].active = trk[k].v[i].gate = 0;
    take_t tk = {0};
    tk.finite = 1;
    render(&tk, 8, 0, 0);
}

static const uint8_t CHORD[6] = {48, 55, 60, 64, 67, 71};
/* part's sound: JUNO preset k (or another engine's), notes held for `hold` blocks then released for `rel` */
static take_t play(uint32_t part, uint32_t e, uint32_t k, uint32_t nnotes, uint32_t hold, uint32_t rel, FILE *w)
{
    take_t tk = {0};
    track_t *t = &trk[part];
    silence();
    host_preset(t, e, k);
    if (part == DOCK_LEAD || part == DOCK_BASS) t->p[P_VOICE] = V_LEGATO;
    for (uint32_t i = 0; i < nnotes; i++)
        trk_note_on(t, nnotes == 1u ? 69u : CHORD[i], 100);
    tk.finite = 1;
    render(&tk, hold, part, w);
    for (uint32_t i = 0; i < nnotes; i++)
        trk_note_off(t, nnotes == 1u ? 69u : CHORD[i]);
    render(&tk, rel, part, w);
    return tk;
}

static FILE *wav_open(const char *dir, const char *name)
{
    char path[512];
    if (!dir) return 0;
    snprintf(path, sizeof path, "%s/juno_%s.wav", dir, name);
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

/* host time of `voices` voices of engine e (preset k) on Keys, ns per block, best of three */
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
        for (uint32_t b = 0; b < 4000; b++) mix_block(o, CTL);
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

    /* Silk Lead: JUNO's lead (no longer Yacht's: the CZ-1 ELEC.PIANO was picked by ear for it) */
    str_cpy(name, eng_preset(ENGINES[ENGI_JUNO], JUNO_SILK_LEAD)->name, 21);
    check("JUNO_SILK_LEAD is SILK LEAD", str_eq(name, "SILK LEAD"));
    check("JUNO can be picked (PRESETS, the editor)", eng_ok(ENGI_JUNO) && eng_rank(ENGI_JUNO) < NENG_SHOWN &&
                                                      eng_vis(eng_rank(ENGI_JUNO)) == ENGI_JUNO);

    /* every sound: it sounds, stays finite, and below clipping (the Lead's mono sounds one note, the rest a chord) */
    int all_sound = 1, all_finite = 1, all_clean = 1;
    for (uint32_t k = 0; k < JUNO_NPRESETS; k++) {
        const juno_patch_t *jp = &JUNO_PATCH[k];
        uint32_t part = jp->mono ? DOCK_LEAD : DOCK_KEYS, nn = jp->mono ? 1u : 6u;
        FILE *w = wav_open(dir, jp->name);
        take_t tk = play(part, ENGI_JUNO, k, nn, (uint32_t)(2.5 * FS / CTL), (uint32_t)(1.5 * FS / CTL), w);
        wav_close(w);
        double rms = sqrt(tk.l2 / tk.n);
        printf("      %-12s peak %5d  rms %7.1f  width %5.3f\n", jp->name, tk.peak, rms,
               tk.l2 + tk.r2 > 0 ? sqrt(tk.s2 / (tk.l2 + tk.r2)) : 0.0);
        all_sound &= rms > 300.0;
        all_finite &= tk.finite;
        all_clean &= tk.peak < 32000;
    }
    check("every sound plays (rms above -40 dBFS)", all_sound);
    check("no NaN or runaway filter state in any sound", all_finite);
    check("no sound clips (peak below 32000 of 32767)", all_clean);

    /* the filter: CUT down darkens (less first-difference energy against the level) */
    {
        take_t open_ = play(DOCK_KEYS, ENGI_JUNO, 0, 1, 200, 10, 0);
        host_preset(&trk[DOCK_KEYS], ENGI_JUNO, 0);
        silence();
        host_preset(&trk[DOCK_KEYS], ENGI_JUNO, 0);
        trk[DOCK_KEYS].p[P_E5] = 50;
        take_t shut = {0};
        shut.finite = 1;
        trk_note_on(&trk[DOCK_KEYS], 69, 100);
        render(&shut, 200, DOCK_KEYS, 0);
        trk_note_off(&trk[DOCK_KEYS], 69);
        check("CUT 50 is darker than CUT 127", shut.d2 / shut.l2 < 0.25 * open_.d2 / open_.l2);
    }
    /* full resonance with the filter half open: it rings, stays bounded */
    {
        silence();
        host_preset(&trk[DOCK_KEYS], ENGI_JUNO, 0);
        trk[DOCK_KEYS].p[P_E5] = 80;
        trk[DOCK_KEYS].p[P_E6] = 127;
        trk[DOCK_KEYS].p[P_E2] = 127;
        take_t tk = {0};
        tk.finite = 1;
        for (uint32_t i = 0; i < 6u; i++) trk_note_on(&trk[DOCK_KEYS], CHORD[i], 127);
        render(&tk, 400, DOCK_KEYS, 0);
        check("RES 127 on a six-note chord: finite, no clipping", tk.finite && tk.peak < 32000);
    }

    /* the chorus: OFF and I+II mono (L == R), I and II stereo (a clear L - R) */
    {
        double width[4];
        int mono[4];
        for (uint32_t c = 0; c < 4u; c++) {
            silence();
            host_preset(&trk[DOCK_KEYS], ENGI_JUNO, 4);           /* WIDE PAD */
            trk[DOCK_KEYS].p[P_E7] = (int16_t)c;
            trk[DOCK_KEYS].p[P_ATK] = 0;
            take_t tk = {0};
            tk.finite = 1;
            for (uint32_t i = 0; i < 3u; i++) trk_note_on(&trk[DOCK_KEYS], CHORD[i + 2u], 100);
            render(&tk, (uint32_t)(2.0 * FS / CTL), DOCK_KEYS, 0);
            width[c] = sqrt(tk.s2 / (tk.l2 + tk.r2));
            mono[c] = width[c] < 0.002;
            if (c == 0u || c == 3u) printf("      (L - R energy ratio %.3g)\n", tk.s2 / (tk.l2 + tk.r2));
            printf("      chorus %-4s width %5.3f  level %7.1f\n", N_JUNO_CHORUS[c], width[c], sqrt((tk.l2 + tk.r2) / 2 / tk.n));
        }
        check("chorus OFF: mono (L - R below -54 dB)", mono[0]);
        check("chorus I: stereo (L - R at least -14 dB of L + R)", width[1] > 0.2);
        check("chorus II: stereo", width[2] > 0.2);
        check("chorus I+II: mono, as the Juno-60's (L - R below -54 dB)", mono[3]);
    }

    /* cost: one and six voices of JUNO INIT against PROPHET INIT on Keys (the whole mix_block, other parts idle) */
    {
        double idle = cost(ENGI_JUNO, 0, 0);
        double j1 = cost(ENGI_JUNO, 0, 1) - idle, p1 = cost(ENGI_PROPHET, 0, 1) - idle;
        double j6 = cost(ENGI_JUNO, 0, 6) - idle, p6 = cost(ENGI_PROPHET, 0, 6) - idle;
        silence();
        host_preset(&trk[DOCK_KEYS], ENGI_JUNO, 2);               /* STR MACHINE: chorus I+II, PWM */
        double js = cost(ENGI_JUNO, 4, 6) - idle;                 /* WIDE PAD: chorus II, sub */
        printf("      host ns per 32-sample block: idle mix %.0f; JUNO 1 voice %.0f, 6 voices %.0f (WIDE PAD %.0f);"
               " PROPHET 1 voice %.0f, 6 voices %.0f\n", idle, j1, j6, js, p1, p6);
        printf("      JUNO / PROPHET per voice: %.2f (1 voice), %.2f (6 voices); PROPHET charges %u units, JUNO %u\n",
               j1 / p1, j6 / p6, p5_units(&trk[DOCK_KEYS]), juno_units(&trk[DOCK_KEYS]));
        check("a JUNO voice costs no more than a PROPHET voice on the host", j6 <= p6 * 1.05);
    }
    printf("juno: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
